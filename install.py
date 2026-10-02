#!/usr/bin/env python3
"""Install Linux Native Camera Tweaks as a bg3le plugin.

Copies linux_native_camera_tweaks.so into bg3le's plugins directory
(~/.local/share/bg3le/plugins, or $BG3LE_PLUGINS_DIR), where bg3le loads it
with the game. No launch options change, so Steam can stay open.

    ./install.py              install, or update an existing install
    ./install.py --uninstall  remove the plugin and its saved settings
    ./install.py --dry-run    show what would change
"""

import argparse
import glob
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = "linux_native_camera_tweaks.so"
SETTINGS = "linux_native_camera_tweaks.settings.json"


def data_home():
    return os.environ.get("XDG_DATA_HOME") or os.path.expanduser("~/.local/share")


def plugins_dir():
    return os.environ.get("BG3LE_PLUGINS_DIR") or os.path.join(data_home(), "bg3le", "plugins")


def copy_atomic(src, dst, mode):
    """Copied beside the target and renamed over it, so a running game keeps the old one mapped."""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(dst), prefix=".lnct-")
    os.close(fd)
    shutil.copyfile(src, tmp)
    os.chmod(tmp, mode)
    os.replace(tmp, dst)


def preload_installs():
    """Steam users whose launch options still load the original LD_PRELOAD build."""
    found = []
    for root in ("~/.local/share/Steam", "~/.steam/steam", "~/.steam/root"):
        for config in glob.glob(os.path.join(os.path.expanduser(root), "userdata", "*", "config",
                                             "localconfig.vdf")):
            try:
                with open(config, encoding="utf-8", errors="replace") as f:
                    text = f.read()
            except OSError:
                continue
            real = os.path.realpath(config)
            if "linux_native_camera_tweaks" in text and real not in found:
                found.append(real)
    return found


def main():
    ap = argparse.ArgumentParser(description="Install Linux Native Camera Tweaks as a bg3le plugin.")
    ap.add_argument("--uninstall", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--plugin", default=os.path.join(HERE, PLUGIN),
                    help="the plugin to install (default: %s beside this script)" % PLUGIN)
    args = ap.parse_args()

    target = plugins_dir()
    plugin = os.path.join(target, PLUGIN)
    settings = os.path.join(target, SETTINGS)

    if args.uninstall:
        for path in (plugin, settings):
            if os.path.exists(path):
                print("Removing %s" % path)
                if not args.dry_run:
                    os.remove(path)
        print("Dry run: nothing changed." if args.dry_run
              else "Removed; the next launch runs without it.")
        return

    if not os.path.isfile(args.plugin):
        sys.exit("install: missing %s" % args.plugin)
    if not os.path.isfile(os.path.join(data_home(), "bg3le", "lib", "libbg3le.so")):
        print("Note: bg3le isn't installed; the plugin does nothing without it.\n"
              "  https://www.nexusmods.com/baldursgate3/mods/25431")

    print("Installing %s" % plugin)
    if not args.dry_run:
        copy_atomic(args.plugin, plugin, 0o755)

    for config in preload_installs():
        print("Warning: %s\n  still loads the original LD_PRELOAD version in the game's launch options.\n"
              "  Remove it there, or the camera is patched twice." % config)

    if args.dry_run:
        print("Dry run: nothing changed.")
    else:
        print("Done: the next launch loads it. Settings: %s (written on first launch), or MCM with the "
              "Linux Native Camera Tweaks Settings mod." % settings)


if __name__ == "__main__":
    main()
