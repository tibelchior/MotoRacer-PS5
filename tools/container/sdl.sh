#!/bin/bash
# SDL3 for Prospero: the official release (tools/pins.env) with the port's hooks (ps5/sdl/hooks.patch: dummy
# video/audio output to the console, virtual joystick) and Vulkan (ps5/sdl/patch.py): static library in $OUT/sdl-vk-build.
source "$(dirname "$0")/common.sh"
install_crt
SRC0=$VOL/sdl-$SDL_TAG   # pristine download, kept in the volume
SRC=$OUT/sdl-src         # patched copy
[ -d "$SRC0/.git" ] || git clone -q --depth 1 --branch "$SDL_TAG" "$SDL_REPO" "$SRC0"
mkdir -p "$OUT"
B=$OUT/sdl-vk-build; mkdir -p "$B"
rm -rf "$SRC"; cp -r "$SRC0" "$SRC"
git -C "$SRC" apply --whitespace=nowarn /work/ps5/sdl/hooks.patch
python3 /work/ps5/sdl/patch.py "$SRC" /work/ps5/sdl/nullvulkan.inc
# rebuild when the patch (or the SDL source it patches) changed
STAMP=$(cat /work/ps5/sdl/hooks.patch /work/ps5/sdl/patch.py /work/ps5/sdl/nullvulkan.inc | md5sum | cut -d' ' -f1)
if [ -f "$B/libSDL3.a" ] && [ "$(cat "$B/.patch-stamp" 2>/dev/null)" = "$STAMP" ]; then log "SDL3 already built"; exit 0; fi
rm -f "$B/libSDL3.a"; echo "$STAMP" > "$B/.patch-stamp"
log "SDL3 configure"
cmake -S "$SRC" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$PS5_SDK_DIR/toolchain/prospero.cmake" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF \
    -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=ON -DSDL_GPU=ON -DSDL_RENDER=OFF \
    -DSDL_DUMMYVIDEO=ON -DSDL_OFFSCREEN=OFF -DSDL_DUMMYAUDIO=ON -DSDL_DISKAUDIO=OFF \
    -DSDL_OSS=OFF -DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
    -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_LIBUDEV=OFF -DSDL_HIDAPI=OFF -DSDL_VIRTUAL_JOYSTICK=ON -DSDL_LIBUSB=OFF -DSDL_CAMERA=OFF \
    -DSDL_DIALOG=OFF -DSDL_TRAY=OFF > "$OUT/sdl-configure.log" 2>&1 || { tail -30 "$OUT/sdl-configure.log"; exit 1; }
log "SDL3 build"
cmake --build "$B" > "$OUT/sdl-build.log" 2>&1 || { grep -E 'error|FAILED' "$OUT/sdl-build.log" | head -30; exit 1; }
ls -la "$B/libSDL3.a"
