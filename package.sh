#!/usr/bin/env bash
# Builds dist/: the plugin against the Steam Runtime sniper sysroot (so it loads on any distribution) and the MCM
# settings mod's pak, each in its own zip. Install only with the game closed.
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

# Two downloads, published separately: the plugin, and the optional MCM page.
VERSION="$(grep -o '#define VERSION "[^"]*"' "$ROOT/src/main.c" | cut -d'"' -f2)"
python3 - "$DIST" "$ROOT" "$VERSION" <<'PY'
import os, re, sys, zipfile
dist, root, version = sys.argv[1:]
v = int(re.search(r'id="Version64" type="int64" value="(\d+)"',
                  open(os.path.join(root, "mod/Mods/LNCTSettings/meta.lsx")).read()).group(1))
pak_version = f"{v >> 55}.{(v >> 47) & 0xFF}.{(v >> 31) & 0xFFFF}.{v & 0x7FFFFFFF}"
for name, files in ((f"LinuxNativeCameraTweaks-{version}.zip", ["linux_native_camera_tweaks.so"]),
                    (f"LNCTSettings-{pak_version}.zip", ["LNCTSettings.pak"])):
    with zipfile.ZipFile(os.path.join(dist, name), "w", zipfile.ZIP_DEFLATED) as z:
        for f in files:
            z.write(os.path.join(dist, f), f)
        z.write(os.path.join(root, "README.md"), "README.md")
PY
ls -lh "$DIST"
