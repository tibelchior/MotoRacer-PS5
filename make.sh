#!/bin/bash
# One command from your GOG installer to a PS5-ready folder. You only need Docker (running).
#   ./make.sh /path/to/setup_moto_racer_2.1.0.11.exe
#   ./make.sh /path/to/installed/game/folder          (a folder that holds game.EXE)
# Result: dist/PPSA99785/ -> copy to /data/homebrew/PPSA99785 on the console.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
die() { echo "error: $*" >&2; exit 1; }
step() { printf '\n######## %s\n' "$*"; }
[ $# -eq 1 ] || die "usage: ./make.sh <GOG installer .exe | folder with game.EXE>"
[ -e "$1" ] || die "not found: $1"
command -v docker > /dev/null || die "Docker is not installed (https://docs.docker.com/get-docker/)"
docker info > /dev/null 2>&1 || die "Docker is not running: start it and run this again"
command -v rsync > /dev/null || die "rsync is not installed"
export BUILD_IMAGE=${BUILD_IMAGE:-motoracerps5-build}

step "1/5 build environment"
docker build -q -t "$BUILD_IMAGE" "$HERE/docker" > /dev/null

step "2/5 your game files"
if [ -d "$1" ]; then
    GAME=$(cd "$1" && pwd)
else
    dir=$(cd "$(dirname "$1")" && pwd)
    rm -rf "$HERE/game-extract"; mkdir -p "$HERE/game-extract"
    docker run --rm -v "$dir":/in:ro -v "$HERE":/work "$BUILD_IMAGE" \
        innoextract --gog --silent -d /work/game-extract "/in/$(basename "$1")" || die "could not unpack the installer"
    GAME=$HERE/game-extract/app
    [ -f "$GAME/game.EXE" ] || GAME=$HERE/game-extract
fi
[ -f "$GAME/game.EXE" ] || die "no game.EXE in $GAME (is this the GOG release of Moto Racer?)"
export GAME_DIR=$GAME

step "3/5 PS5 toolchain (1 to 2 hours the first time, then cached)"
"$HERE/build.sh" toolchain
step "4/5 translating your game.EXE and building the title"
"$HERE/build.sh" gen
"$HERE/build.sh" title
step "5/5 copying your game files into the title"
"$HERE/build.sh" package
echo
echo "Ready: $HERE/dist/PPSA99785"
echo "Copy that folder to /data/homebrew/PPSA99785 on your PS5 and start Moto Racer."
