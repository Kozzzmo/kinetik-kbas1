#!/bin/sh
# Files shared with the other Kinetik plug-ins (kinetik-kdsp16, kinetik-kmsp6) must stay identical.
#   scripts/check-shared.sh                 check them against scripts/shared.sha256 (CI does this)
#   scripts/check-shared.sh ../kinetik-kdsp16  compare with a sister repository, then refresh the list
set -e
cd "$(dirname "$0")/.."
FILES="src/Skins.hpp src/Fonts.hpp src/Dsp.hpp src/Updater.hpp src/Updater.cpp src/License.hpp src/FsUtf8.hpp src/LogoMark.hpp
src/third_party/monocypher/monocypher.c src/third_party/monocypher/monocypher.h src/third_party/monocypher/monocypher-ed25519.c
src/third_party/monocypher/monocypher-ed25519.h fonts/OFL-Unbounded.txt"
if [ -n "$1" ]; then
    bad=0
    for f in $FILES; do
        if ! cmp -s "$f" "$1/$f"; then echo "differs: $f"; bad=1; fi
    done
    [ $bad = 0 ] || { echo "copy the newer version to both repositories"; exit 1; }
    sha256sum $FILES > scripts/shared.sha256
    echo "identical; scripts/shared.sha256 refreshed (copy it to the sister repository too)"
else
    sha256sum -c --quiet scripts/shared.sha256 || {
        echo "::error::a file shared with the sister plug-in changed: update it in both repositories, then run scripts/check-shared.sh <sister repo>"
        exit 1; }
    echo "shared files match scripts/shared.sha256"
fi
