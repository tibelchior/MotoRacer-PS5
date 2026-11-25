// PS5 adapters. The guest's 4 GB window is a reserved virtual range; committed 64 KB chunks get direct
// memory mapped in (as in I76-PS5 / DiRT3, ps5platform/shm.h). In a process without direct memory
// (a payload) it is one anonymous mapping. Files are plain POSIX; everything lives in the data folder.
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include <ps5platform/kernel.h>
#include <ps5platform/shm.h>

#include "platform.h"
#include "runtime.h"
#include "buffered_file.h"

extern "C" void call_on_stack(void *stack_top, void (*fn)(void));   // stack_x86_64_sysv.S
extern "C" const char *mh_ps5_data_dir(void);                       // ps5_launch.cpp

namespace plat {

namespace {

constexpr uint64_t kChunk = 0x10000;       // direct-memory unit
constexpr uint64_t kMaxRun = 64ull << 20;  // one direct-memory allocation at most

class Ps5VirtualMemory final : public VirtualMemory {
public:
    uint8_t *reserve(uint64_t size) override {
        direct_ = sceKernelGetDirectMemorySize() > 0;
        void *m = nullptr;
        if (direct_) {
            if (ps5_vrange_reserve(size, nullptr, 0, &m) || !m) return nullptr;
            rt::log("guest memory: 4 GB reserved, direct memory backs committed chunks\n");
        } else {
            m = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (m == MAP_FAILED) return nullptr;
            rt::log("guest memory: anonymous mapping (no direct memory in this process)\n");
        }
        base_ = static_cast<uint8_t *>(m);
        mapped_.assign(size / kChunk, false);
        return base_;
    }
    bool commit(void *addr, uint64_t size) override {
        if (!direct_ || !size) return true;
        std::lock_guard<std::mutex> lk(mu_);
        const uint64_t off = static_cast<uint8_t *>(addr) - base_;
        uint64_t c = off / kChunk;
        const uint64_t end = (off + size + kChunk - 1) / kChunk;
        while (c < end) {
            if (mapped_[c]) { ++c; continue; }
            uint64_t e = c;
            while (e < end && !mapped_[e] && (e - c) * kChunk < kMaxRun) ++e;
            uint8_t *const at = base_ + c * kChunk;
            const size_t n = static_cast<size_t>(e - c) * kChunk;
            ps5_shm backing{};
            void *view = nullptr;
            int rc = ps5_shm_create(n, &backing);
            if (!rc) {
                rc = ps5_shm_map(&backing, 0, n, at, PS5_SHM_READ | PS5_SHM_WRITE, PS5_SHM_FIXED, &view);
                if (!rc && view != at) rc = PS5_SHM_NO_PLACE;
            }
            if (rc) {
                rt::log("guest commit 0x%llx+0x%zx failed: 0x%x (%llu MB backed)\n", (unsigned long long)(c * kChunk), n,
                        unsigned(rc), (unsigned long long)(backed_ >> 20));
                return false;
            }
            memset(at, 0, n);   // direct memory is handed out as it was released
            for (uint64_t k = c; k < e; ++k) mapped_[k] = true;
            backed_ += n;
            c = e;
        }
        return true;
    }
    bool make_inaccessible(void *addr, uint64_t size) override {
        if (direct_) return true;   // unbacked chunks of the reservation fault already
        return mprotect(addr, size, PROT_NONE) == 0;
    }

private:
    bool direct_ = false;
    uint8_t *base_ = nullptr;
    std::vector<bool> mapped_;
    uint64_t backed_ = 0;
    std::mutex mu_;
};

class Ps5Execution final : public Execution {
public:
    // The recompiled code nests one host frame per guest call: a big stack from the title heap (direct
    // memory; libkernel's own thread stacks come out of a ~400 MB flexible budget).
    bool run_on_large_stack(uint64_t bytes, void (*fn)()) override {
        for (uint64_t b = bytes; b >= (64ull << 20); b /= 2) {
            void *stack = nullptr;
            if (posix_memalign(&stack, 0x4000, b) == 0 && stack) {
                rt::log("host stack: %llu MB\n", (unsigned long long)(b >> 20));
                call_on_stack(static_cast<char *>(stack) + b, fn);
                return true;
            }
        }
        return false;
    }
    bool start_sampler(uint32_t interval_us, void (*tick)()) override {
        std::thread([interval_us, tick] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
                tick();
            }
        }).detach();
        return true;
    }
};

void (*fault_handler)(const Fault &) = nullptr;

void on_signal(int sig, siginfo_t *si, void *) {
    Fault f{};
    f.code = sig;
    if (sig == SIGSEGV || sig == SIGBUS) {
        f.kind = Fault::MemoryAccess;
        f.address = reinterpret_cast<uintptr_t>(si->si_addr);
    } else {
        f.kind = sig == SIGABRT ? Fault::Abort : Fault::Other;
    }
    fault_handler(f);
    _exit(2);
}

class Ps5CrashReporter final : public CrashReporter {
public:
    void install(void (*on_fault)(const Fault &)) override {
        fault_handler = on_fault;
        static char altstack[1 << 16];
        stack_t ss{};
        ss.ss_sp = altstack;
        ss.ss_size = sizeof altstack;
        sigaltstack(&ss, nullptr);
        struct sigaction sa{};
        sa.sa_sigaction = on_signal;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE}) sigaction(sig, &sa, nullptr);
    }
    void write_backtrace(FILE *) override {}   // no execinfo on the console; addresses come from the fault line
};

FileInfo info_from(const struct stat &st) {
    FileInfo i;
    i.exists = true;
    i.is_dir = S_ISDIR(st.st_mode);
    i.size = static_cast<uint64_t>(st.st_size);
    i.mtime = static_cast<int64_t>(st.st_mtime);
    return i;
}

class Ps5File final : public File {
public:
    explicit Ps5File(int fd) : fd_(fd) {}
    ~Ps5File() override { close(fd_); }
    int64_t read(void *dst, uint32_t n) override { return ::read(fd_, dst, n); }
    int64_t write(const void *src, uint32_t n) override { return ::write(fd_, src, n); }
    int64_t seek(int64_t offset, Seek from) override {
        int whence = from == Seek::Begin ? SEEK_SET : from == Seek::Current ? SEEK_CUR : SEEK_END;
        return lseek(fd_, offset, whence);
    }
    bool truncate_at_position() override { return ftruncate(fd_, lseek(fd_, 0, SEEK_CUR)) == 0; }
    FileInfo info() override {
        struct stat st;
        return fstat(fd_, &st) == 0 ? info_from(st) : FileInfo{};
    }

private:
    int fd_;
};

class Ps5FileSystem final : public FileSystem {
public:
    std::unique_ptr<File> open(const std::string &path, bool rd, bool wr, Disposition disp, FileError *err) override {
        int flags = wr ? (rd ? O_RDWR : O_WRONLY) : O_RDONLY;
        switch (disp) {
        case Disposition::CreateNew: flags |= O_CREAT | O_EXCL; break;
        case Disposition::CreateAlways: flags |= O_CREAT | O_TRUNC; break;
        case Disposition::OpenAlways: flags |= O_CREAT; break;
        case Disposition::TruncateExisting: flags |= O_TRUNC; break;
        case Disposition::OpenExisting: break;
        }
        int fd = ::open(path.c_str(), flags, 0644);
        if (fd < 0) {
            if (err) *err = errno == EEXIST ? FileError::Exists : errno == EACCES ? FileError::AccessDenied : FileError::NotFound;
            return nullptr;
        }
        if (err) *err = FileError::None;
        return buffered_if_read_only(std::make_unique<Ps5File>(fd), wr);
    }
    FileInfo stat(const std::string &path) override {
        struct stat st;
        return ::stat(path.c_str(), &st) == 0 ? info_from(st) : FileInfo{};
    }
    std::vector<std::string> list_dir(const std::string &path) override {
        std::vector<std::string> names;
        if (DIR *d = opendir(path.empty() ? "." : path.c_str())) {
            while (dirent *e = readdir(d)) {
                std::string n = e->d_name;
                if (n != "." && n != "..") names.push_back(std::move(n));
            }
            closedir(d);
        }
        return names;
    }
    bool make_dir(const std::string &path) override { return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST; }
    bool remove_file(const std::string &path) override { return unlink(path.c_str()) == 0; }
    bool rename(const std::string &from, const std::string &to) override { return ::rename(from.c_str(), to.c_str()) == 0; }
    bool set_current_dir(const std::string &path) override { return chdir(path.c_str()) == 0; }
};

class Ps5Paths final : public HostPaths {
public:
    std::string data_dir() override { return mh_ps5_data_dir(); }
    std::string log_dir() override { return mh_ps5_data_dir(); }
};

class Ps5HostInfo final : public HostInfo {
public:
    bool is_steam_deck() override { return false; }
    bool pad_only() override { return true; }
};

class Ps5Modules final : public Modules {
public:
    void *open(const std::string &, std::string *error) override {
        if (error) *error = "not supported on the PS5";
        return nullptr;
    }
    void *symbol(void *, const char *) override { return nullptr; }
    const char *suffix() override { return ".so"; }
};

}  // namespace

Host &host() {
    static Ps5VirtualMemory memory;
    static Ps5Execution execution;
    static Ps5CrashReporter crash;
    static Ps5FileSystem fs;
    static Ps5Paths paths;
    static Ps5HostInfo info;
    static Ps5Modules modules;
    static Host h{memory, execution, crash, fs, paths, info, modules};
    return h;
}

void register_launchers(const std::string &, const std::string &) {}

}  // namespace plat
