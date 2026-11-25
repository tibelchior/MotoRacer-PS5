#!/bin/bash
# Packs the built title into dist/MotoRacer-PS5-v<version>-private.zip: the title only, with none of the game's
# files. FOR YOUR OWN USE: eboot.bin holds your game.EXE translated to native code, so do not share this zip
# (the shareable one is tools/package-release.sh).
#   tools/package-private.sh 1.0.0 [built title folder, default dist/PPSA99785]
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
source "$HERE/tools/pins.env"
VERSION=${1:?usage: tools/package-private.sh <version> [title folder]}
SRC=${2:-$HERE/dist/$TITLE_ID}
[ -f "$SRC/eboot.bin" ] || { echo "error: no title in $SRC: run ./build.sh title first" >&2; exit 1; }
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$TITLE_ID/MotoRacer"
cp "$SRC/eboot.bin" "$STAGE/$TITLE_ID/"
cp -R "$SRC/sce_sys" "$SRC/sce_module" "$STAGE/$TITLE_ID/"
cat > "$STAGE/$TITLE_ID/MotoRacer/PUT_THE_GAME_FILES_HERE.txt" <<'TXT'
Copy your Moto Racer (GOG) game into this folder: everything in the folder that holds game.EXE, including
  game.EXE, data/, 02.wav ... 13.wav (the music), motoracer.ini
Not needed: other .exe files, goggame*, __support/, GameuxInstallHelper.dll, readme and FAQ text files.

Use the same game.EXE this title was built from (GOG setup_moto_racer_2.1.0.11).
TXT
find "$STAGE" -name .DS_Store -delete
OUT=$HERE/dist/MotoRacer-PS5-v$VERSION-private.zip
rm -f "$OUT"
(cd "$STAGE" && zip -qr "$OUT" "$TITLE_ID")
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
unzip -l "$OUT" | tail -n +4
