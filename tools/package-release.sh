#!/bin/bash
# Packs the port kit into dist/MotoRacer-PS5-v<version>.zip: sources and scripts only. No game files and no
# built title: the title holds the player's game.EXE translated to native code, so each player builds their own.
#   tools/package-release.sh 1.0.0
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
VERSION=${1:?usage: tools/package-release.sh <version>}
NAME=MotoRacer-PS5-v$VERSION
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$NAME" "$HERE/dist"
cd "$HERE"
# what git tracks or would track: .gitignore keeps the game, the translated code and the builds out
git ls-files -co --exclude-standard -z | rsync -a --files-from=- --from0 ./ "$STAGE/$NAME/"
bad=$(cd "$STAGE/$NAME" && find . \( -path './repo/gen*' -o -path './dist' -o -path './game-extract' -o -iname '*.exe' -o -name eboot.bin \))
[ -z "$bad" ] || { echo "error: files that must not ship:" >&2; echo "$bad" >&2; exit 1; }
find "$STAGE" -name .DS_Store -delete
OUT=$HERE/dist/$NAME.zip
rm -f "$OUT"
(cd "$STAGE" && zip -qr "$OUT" "$NAME")
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
