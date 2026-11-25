// PS5: the Direct3D execute-buffer backend is not built (software renderer only, first milestone).
// Every entry point says "not handled", so the DirectDraw / Direct3D shims fall back to the CPU paths.
#include "dx/d3d_gpu.h"

namespace dx::gpu {
bool init() { return false; }
void set_target(Surface *) {}
uint32_t add_vertices(const TLVertex *, uint32_t) { return 0; }
void execute_buffer_locked(uint32_t, uint32_t) {}
bool direct_span(uint32_t, uint32_t, DirectSpan *) { return false; }
bool direct_process(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *) { return false; }
void execute_done(uint32_t) {}
void draw(const uint32_t *, Surface *, uint32_t, uint32_t, const uint16_t *, uint32_t) {}
void clear(uint32_t, uint32_t, float) {}
void sync_to_cpu(Surface *) {}
void cpu_wrote(Surface *) {}
void surface_destroyed(Surface *) {}
bool present_flip(Surface *) { return false; }
bool present_blt(Surface *, Surface *, int32_t, int32_t, uint32_t, uint32_t) { return false; }
}  // namespace dx::gpu
