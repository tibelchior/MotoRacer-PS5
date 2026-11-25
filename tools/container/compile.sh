#!/bin/bash
# Cross-compiles the generated game code and the runtime as objects for Prospero (title.sh links).
source "$(dirname "$0")/common.sh"
install_crt
SRC=$OUT/sdl-src
B=$OUT/cmake; mkdir -p "$B"
# objects of files that are not part of this configuration (a stale one breaks the link)
rm -f "$B"/CMakeFiles/motoracer.dir/runtime/src/ps5/*.o
log "configure"
cmake -S /work/repo -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPS5=ON -DCMAKE_TOOLCHAIN_FILE="$PS5_SDK_DIR/toolchain/prospero.cmake" \
    -DMOTORACER_PS5_SDL_INCLUDE="$SRC/include" -DMOTORACER_PS5_SDL_LIB="$OUT/sdl-vk-build/libSDL3.a" -DGAME_EXE=/nonexistent/game.EXE \
    > "$OUT/configure.log" 2>&1 || { tail -40 "$OUT/configure.log"; exit 1; }
log "build"
cmake --build "$B" -- -k 0 > "$OUT/build.log" 2>&1 || { grep -E 'error:|FAILED' "$OUT/build.log" | sort | uniq -c | sort -rn | head -40; echo "see $OUT/build.log (in the volume)"; exit 1; }
find "$B" -name '*.o' | wc -l
