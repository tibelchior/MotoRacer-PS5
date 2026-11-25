// PS5: no SDL_GPU. The software renderer's RGBA frame goes to the console's display through the
// RADV presenter (ps5/glue/ps5_present.cpp), which keeps the aspect and adds black bars.
#include <algorithm>
#include <cstdint>
#include <vector>

#include "host/host_gpu.h"
#include "runtime.h"

extern "C" void mh_ps5_present(const void *pixels, int width, int height, int pitch);   // ps5_present.cpp

namespace host::gpu {

namespace { uint32_t game_w = 640, game_h = 480; }

SDL_GPUDevice *device() { return nullptr; }
void apply_vsync() {}
void present(SDL_GPUCommandBuffer *, SDL_GPUTexture *, uint32_t, uint32_t, const Crop &) {}
void present_placed(SDL_GPUCommandBuffer *, SDL_GPUTexture *, uint32_t, uint32_t, const Crop &, const Crop &) {}
uint32_t render_scale(uint32_t, uint32_t) { return 1; }
void set_game_size(uint32_t w, uint32_t h) { game_w = w; game_h = h; }

Crop widescreen_vote(bool bars_black, uint32_t gw, uint32_t gh) {
    static int score = 0;
    Crop full{0, 0, gw, gh};
    if (gh * 4 != gw * 3) { score = 0; return full; }   // only the 4:3 frame has a 16:9 band
    score = bars_black ? std::min(score + 1, 3) : 0;
    if (score < 2) return full;
    const uint32_t band = gw * 9 / 16;   // 640 -> 360
    return Crop{0, (gh - band) / 2, gw, band};
}

void present_rgba(const uint32_t *px, uint32_t w, uint32_t h, const Crop &crop) {
    game_w = w;
    game_h = h;
    const uint32_t cw = std::min(crop.w, w - std::min(crop.x, w)), ch = std::min(crop.h, h - std::min(crop.y, h));
    if (!cw || !ch) return;
    // host_video packs XRGB already (MOTORACER_PS5): the whole frame goes straight to the presenter, a crop
    // (the 16:9 band) is just a pointer and a row pitch.
    mh_ps5_present(px + static_cast<size_t>(crop.y) * w + crop.x, static_cast<int>(cw), static_cast<int>(ch), static_cast<int>(w * 4));
}

bool window_to_game(float wx, float wy, int *gx, int *gy) {
    *gx = std::clamp(static_cast<int>(wx), 0, static_cast<int>(game_w) - 1);
    *gy = std::clamp(static_cast<int>(wy), 0, static_cast<int>(game_h) - 1);
    return true;
}

}  // namespace host::gpu
