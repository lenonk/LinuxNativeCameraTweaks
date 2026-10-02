#!/usr/bin/env bash
# Builds dist/: the plugin against the Steam Runtime sniper sysroot (so it loads on any distribution) and the MCM
# settings mod's pak. Install only with the game closed.
#   BG3LE_SNIPER_SYSROOT  sniper sysroot, as made by bg3le's tools/build-sniper.sh (default: ~/bg3mods/bg3le's)
#   BG3TOOL               bg3tool.dll, to pack the pak
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BG3LE="${BG3LE:-$HOME/bg3mods/bg3le}"
export BG3LE_SNIPER_SYSROOT="${BG3LE_SNIPER_SYSROOT:-$BG3LE/build-sniper/sysroot}"
BG3TOOL="${BG3TOOL:-$HOME/bg3mods/Martyr_Valdas_BG3/tools/bg3tool/out/bg3tool.dll}"
DIST="$ROOT/dist"
mkdir -p "$DIST"

cmake -S "$ROOT" -B "$ROOT/build-sniper" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$BG3LE/cmake/sniper-toolchain.cmake"
cmake --build "$ROOT/build-sniper"
newest="$(objdump -T "$ROOT/build-sniper/linux_native_camera_tweaks.so" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)"
echo "plugin needs $newest"
cp "$ROOT/build-sniper/linux_native_camera_tweaks.so" "$DIST/"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -r "$ROOT/mod/Mods" "$STAGE/"
dotnet "$BG3TOOL" pack "$STAGE" "$DIST/LNCTSettings.pak.tmp"
mv -f "$DIST/LNCTSettings.pak.tmp" "$DIST/LNCTSettings.pak"

# One download: the plugin, its MCM settings pak and the README.
VERSION="$(grep -o '#define VERSION "[^"]*"' "$ROOT/src/main.c" | cut -d'"' -f2)"
python3 - "$DIST" "$ROOT/README.md" "LinuxNativeCameraTweaks-$VERSION.zip" <<'PY'
import os, sys, zipfile
dist, readme, name = sys.argv[1:]
with zipfile.ZipFile(os.path.join(dist, name), "w", zipfile.ZIP_DEFLATED) as z:
    for f in ("linux_native_camera_tweaks.so", "LNCTSettings.pak"):
        z.write(os.path.join(dist, f), f)
    z.write(readme, "README.md")
PY
ls -lh "$DIST"
