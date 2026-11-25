#!/bin/bash
# Builds Moto Racer for the PS5 from the recompiled game (repo/gen-ps5) and your own GOG files.
#   ./build.sh gen       translate game.EXE to C on the host (needs repo/.venv); writes repo/gen-ps5
#   ./build.sh sdl       SDL3 for Prospero (patched source from the Motorhead port's tree in the volume)
#   ./build.sh compile   cross-compile runtime + game code
#   ./build.sh title     sdl + compile + link + sign into dist/<TITLE_ID>
#   ./build.sh package   copy the GOG game folder into dist/<TITLE_ID>/MotoRacer and add port settings
# Options: BUILD_VOLUME=<name> (default dirt2ps5-build-data), GAME_DIR=<GOG game folder (game.EXE inside)>
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
source "$HERE/tools/pins.env"
STEP=${1:-title}
GAME=${GAME_DIR:-$HERE/game-extract/app}
IMAGE=dirt2ps5-build VOLUME=${BUILD_VOLUME:-dirt2ps5-build-data} NAME=motoracerps5
die() { echo "error: $*" >&2; exit 1; }
if [ "$STEP" = gen ]; then
    [ -f "$GAME/game.EXE" ] || die "no $GAME/game.EXE"
    mkdir -p "$HERE/repo/gen-ps5"
    "$HERE/repo/.venv/bin/python" -I "$HERE/repo/recompiler/recomp_main.py" "$GAME/game.EXE" "$HERE/repo/gen-ps5" --files 64
    exit 0
fi
docker info > /dev/null 2>&1 || die "Docker is not running."
docker image inspect "$IMAGE" > /dev/null 2>&1 || docker build -q -t "$IMAGE" "$HERE/docker" > /dev/null
docker rm -f "$NAME" > /dev/null 2>&1 || true
mkdir -p "$HERE/dist" "$HERE/art"
if [ "$STEP" != package ]; then
    docker run -d --name "$NAME" -v "$VOLUME":/build -v "$HERE":/work "$IMAGE" sleep infinity > /dev/null
    trap 'docker rm -f "$NAME" > /dev/null 2>&1 || true' EXIT
fi
run() { docker exec -e VOL=/build "$NAME" bash "/work/tools/container/$1.sh"; }
case $STEP in
    sdl) run sdl ;;
    compile) run sdl; run compile ;;
    title) run sdl; run compile || true; run title ;;
    package)
        OUT=$HERE/dist/$TITLE_ID
        [ -f "$OUT/eboot.bin" ] || die "no title yet: run ./build.sh title first"
        [ -f "$GAME/game.EXE" ] || die "no $GAME/game.EXE (set GAME_DIR)"
        mkdir -p "$OUT/MotoRacer"
        rsync -a --exclude '*.exe' --exclude 'goggame*' --exclude '__support' --exclude 'GameuxInstallHelper.dll' --exclude '/readme*.txt' --exclude '/lisezmoi.txt' --exclude '/faq_*' "$GAME/" "$OUT/MotoRacer/"
        cp "$GAME/game.EXE" "$OUT/MotoRacer/"
        # the CD audio tracks are the GOG WAVs (02.wav..13.wav) the runtime plays through its MCI emulation
        echo "Done: $OUT  ->  copy to /data/homebrew/$TITLE_ID on the console (read data/motoracer.log first)" ;;
    *) die "unknown step '$STEP'" ;;
esac
