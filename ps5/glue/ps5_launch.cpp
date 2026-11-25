// Launch glue for the installed title. The console mounts the title folder as /app0:
//   /app0/MotoRacer/  the GOG game files (the game writes its settings and best times next to game.EXE)
//   /app0/data/       what the port writes: motoracer.log is the one log
//   /app0/data/output optional: 1080, 1440 or 2160 forces the display mode
// If /app0/data cannot be written, /data/motoracer is used instead (the game stays in /app0/MotoRacer).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <dirent.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdarg>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int __real_main(int argc, char **argv);
extern "C" int sceSystemServiceLoadExec(const char *path, char *const argv[]);
extern "C" void mh_ps5_log(const char *text);
extern "C" void mh_ps5_input_start(void);

namespace {
std::string g_data = "/app0/data";
const std::string kGame = "/app0/MotoRacer";   // the GOG files, next to data/

bool writable(const std::string &dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string probe = dir + "/.write-test";
    if (FILE *f = std::fopen(probe.c_str(), "w")) {
        std::fclose(f);
        std::remove(probe.c_str());
        return true;
    }
    return false;
}
}  // namespace

// SDL's base path (SDL patch 0002).
extern "C" const char *mh_ps5_base_path(void) { return "/app0/"; }

// The port's data folder (no trailing slash).
extern "C" const char *mh_ps5_data_dir(void) { return g_data.c_str(); }

// The console refuses chdir() for the title ("cannot chdir to /app0/MotoRacer": no working directory, like its
// getcwd()). The game and the runtime use relative paths, so the link wraps the path calls: after a failed
// chdir the directory is remembered here and relative paths are resolved against it.
extern "C" {
int __real_chdir(const char *);
int __real_open(const char *, int, ...);
FILE *__real_fopen(const char *, const char *);
int __real_stat(const char *, struct stat *);
int __real_lstat(const char *, struct stat *);
DIR *__real_opendir(const char *);
int __real_mkdir(const char *, mode_t);
int __real_unlink(const char *);
int __real_rename(const char *, const char *);
int __real_access(const char *, int);
}
namespace {
std::string g_cwd;   // empty: the real working directory
std::string resolve(const char *path) {
    if (!path || !*path || path[0] == '/' || g_cwd.empty()) return path ? path : "";
    return g_cwd + "/" + path;
}
}  // namespace
extern "C" int __wrap_chdir(const char *path) {
    const int rc = __real_chdir(path);
    if (rc == 0) { g_cwd.clear(); return 0; }
    const int err = errno;
    struct stat st;
    const std::string target = resolve(path);
    if (__real_stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        g_cwd = target;
        std::fprintf(stderr, "[ps5-launch] chdir(%s) failed (errno %d): emulating the working directory\n", path, err);
        return 0;
    }
    errno = err;
    return -1;
}
extern "C" int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = static_cast<mode_t>(va_arg(ap, int)); va_end(ap); }
    return __real_open(resolve(path).c_str(), flags, mode);
}
extern "C" FILE *__wrap_fopen(const char *path, const char *mode) { return __real_fopen(resolve(path).c_str(), mode); }
extern "C" int __wrap_stat(const char *path, struct stat *st) { return __real_stat(resolve(path).c_str(), st); }
extern "C" int __wrap_lstat(const char *path, struct stat *st) { return __real_lstat(resolve(path).c_str(), st); }
extern "C" DIR *__wrap_opendir(const char *path) { return __real_opendir(resolve(path).c_str()); }
extern "C" int __wrap_mkdir(const char *path, mode_t mode) { return __real_mkdir(resolve(path).c_str(), mode); }
extern "C" int __wrap_unlink(const char *path) { return __real_unlink(resolve(path).c_str()); }
extern "C" int __wrap_rename(const char *a, const char *b) { return __real_rename(resolve(a).c_str(), resolve(b).c_str()); }
extern "C" int __wrap_access(const char *path, int mode) { return __real_access(resolve(path).c_str(), mode); }

// The console's getcwd() jumps to a bad address; the title has no working directory (--wrap=getcwd).
extern "C" char *__wrap_getcwd(char *buf, size_t size) {
    const std::string dir = g_cwd.empty() ? kGame : g_cwd;
    if (!buf) { buf = static_cast<char *>(std::malloc(dir.size() + 1)); if (!buf) return nullptr; }
    else if (size < dir.size() + 1) return nullptr;
    std::memcpy(buf, dir.c_str(), dir.size() + 1);
    return buf;
}

// Every exit asks the system to close the title (a plain exit makes the console show an error screen).
extern "C" void __wrap_exit(int code) {
    mh_ps5_log("[ps5-launch] exit requested, asking the system to close the title");
    std::fflush(nullptr);
    usleep(300 * 1000);
    sceSystemServiceLoadExec("exit", nullptr);
    usleep(1500 * 1000);
    _exit(code);
}

// SDL's own messages (the GPU driver says why it fails there) go to the log file.
static void sdl_log(void *, int category, SDL_LogPriority priority, const char *message) {
    if (FILE *f = std::fopen((g_data + "/motoracer.log").c_str(), "a")) {
        std::fprintf(f, "[sdl %d/%d] %s\n", category, int(priority), message);
        std::fclose(f);
    }
    std::fprintf(stderr, "[sdl %d/%d] %s\n", category, int(priority), message);
}

// The title's entry point (app_crt.cpp calls "main", the link wraps it).
extern "C" int __wrap_main(int, char **) {
    if (!writable(g_data)) g_data = "/data/motoracer";
    std::error_code ec;
    std::filesystem::create_directories(g_data, ec);
    // SDL only uses its dummy drivers when asked: frames go to the display through ps5_present.cpp, sound
    // through the audio driver patch. Hints, not environment variables (SDL keeps its own environment copy).
    SDL_SetLogOutputFunction(sdl_log, nullptr);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    // Renderer: the repo's Direct3D backend on the GPU (SDL_GPU over Vulkan), unless data/renderer says "software"
    // or the last GPU start never reached its first frame (marker data/gpu-trying): then the software renderer
    // runs once, with the larger SCREEN MODEs, and the log says why. Delete data/gpu-trying to try the GPU again.
    {
        char word[32] = {};
        if (FILE *f = std::fopen((g_data + "/renderer").c_str(), "r")) {
            if (std::fscanf(f, "%31s", word) != 1) word[0] = 0;
            std::fclose(f);
        }
        const std::string marker = g_data + "/gpu-trying";
        const bool failed_before = std::filesystem::exists(marker, ec);
        const bool software = !std::strcmp(word, "software") || failed_before;
        if (software) {
            setenv("MOTORACER_RENDERER", "software", 1);
            setenv("MOTORACER_SW_MODES", "1", 1);
            std::fprintf(stderr, "[ps5-launch] software renderer (%s)\n", failed_before ? "the last GPU start did not reach a frame" : "data/renderer");
        } else {
            setenv("MOTORACER_RENDERER", "d3d", 1);
            if (FILE *f = std::fopen(marker.c_str(), "w")) { std::fputs("GPU start in progress\n", f); std::fclose(f); }
            SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
            // Internal resolution: the game's 480 lines times a whole scale that covers the output height
            // (data/output = 1080/1440/2160, default 2160 = 4K -> scale 5, 3200x2400). data/scale overrides it
            // (1..8); a lower number is the answer if 4K turns out to be too slow.
            {
                int out_h = 2160, scale = 0;
                if (FILE *f = std::fopen((g_data + "/output").c_str(), "r")) { if (std::fscanf(f, "%d", &out_h) != 1) out_h = 2160; std::fclose(f); }
                if (FILE *f = std::fopen((g_data + "/scale").c_str(), "r")) { if (std::fscanf(f, "%d", &scale) != 1) scale = 0; std::fclose(f); }
                if (scale < 1 || scale > 8) scale = (out_h + 479) / 480;
                char text[8];
                std::snprintf(text, sizeof text, "%d", scale);
                setenv("MOTORACER_RENDER_SCALE", text, 1);
                std::fprintf(stderr, "[ps5-launch] render scale %d (output %d)\n", scale, out_h);
            }
            std::fprintf(stderr, "[ps5-launch] GPU renderer (SDL_GPU over Vulkan)\n");
        }
    }
    std::fprintf(stderr, "[ps5-launch] Moto Racer PS5, data %s\n", g_data.c_str());
    mh_ps5_input_start();
    static const std::string game = kGame;
    static char a0[] = "motoracer", a1[] = "--game-dir";
    static char a2[512];
    std::snprintf(a2, sizeof a2, "%s", game.c_str());
    char *argv2[] = {a0, a1, a2, nullptr};
    const int code = __real_main(3, argv2);
    // Ending the process from inside makes the console report an error; asking the system to close the title
    // is how a game quits.
    mh_ps5_log("[ps5-launch] game finished, asking the system to close the title");
    std::fflush(nullptr);
    usleep(500 * 1000);
    const int rc = sceSystemServiceLoadExec("exit", nullptr);
    std::fprintf(stderr, "[ps5] the system did not close the title: %d\n", rc);
    usleep(1500 * 1000);
    _exit(code);
}
