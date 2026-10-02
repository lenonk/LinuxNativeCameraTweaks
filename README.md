# Linux Native Camera Tweaks (bg3le plugin)

A native Linux mod for **Baldur's Gate 3** that reworks the game's camera: tilt the camera around your character
and zoom without the game's limits.

This is a fork of **Biiinks78**'s [Linux Native Camera Tweaks](https://www.nexusmods.com/baldursgate3/mods/23896),
which does all the camera work. Thank you, Biiinks78. The fork runs it as a [bg3le](https://github.com/lenonk/bg3le)
native plugin instead of an `LD_PRELOAD` library, and makes its tuning adjustable in-game through the
Mod Configuration Menu.

## What the fork changes

- Loaded by bg3le from `~/.local/share/bg3le/plugins`; no launch option changes.
- Settings: pitch sensitivity, inversion and limits; zoom step, inversion and optional limits; controller pitch
  speed and dead zone. They're set in MCM or in a settings file, and kept between launches; see [Settings](#settings).
- No crash when `CameraToggleMouseRotate` was never rebound: `inputconfig_p1.json` only lists changed bindings, so
  the game's default (middle mouse) is used.
- Works on game build 4.76.31.656.

## Install

Needs [bg3le](https://www.nexusmods.com/baldursgate3/mods/25431). It comes as two downloads:

- **Linux Native Camera Tweaks**, the plugin. Unzip it and run `./install.py`, which copies
  `linux_native_camera_tweaks.so` into `~/.local/share/bg3le/plugins/` (`--uninstall` removes it, `--dry-run` shows
  what it would do). Steam can stay open; the game picks it up at its next launch.
- **Linux Native Camera Tweaks Settings**, optional: the MCM page, a normal pak for your mod manager. Without it the
  settings are in a file; see [Settings](#settings).

Remove the original `LD_PRELOAD` version if you have it; the two would patch the camera twice. The installer warns if
your launch options still load it.

## Settings

With MCM, they're under **Linux Native Camera Tweaks** in its menu and apply at once.

Without MCM, edit `~/.local/share/bg3le/plugins/linux_native_camera_tweaks.settings.json` with the game closed. bg3le
writes it, listing every setting, the first time the plugin starts; the pak isn't needed for this.

| Setting | Default | Range | |
|---|---|---|---|
| `roll_sensitivity` | 2.0 | 0.1–10 | degrees of pitch per unit of mouse movement |
| `invert_roll` | false | | swap which way the mouse tilts the camera |
| `roll_min`, `roll_max` | -89, 89 | -89–89 | how far the camera tilts each way |
| `zoom_step` | 0.25 | 0.01–5 | zoom per mouse wheel notch |
| `invert_zoom` | false | | swap which way the wheel zooms |
| `zoom_limit` | false | | keep the zoom between `zoom_min` and `zoom_max` |
| `zoom_min`, `zoom_max` | 1, 100 | 0–200 | the zoom limits, when on |
| `controller_roll_speed` | 2.0 | 0.1–10 | degrees of pitch per frame with the right stick fully over |
| `controller_deadzone` | 4000 | 0–32000 | right stick dead zone, out of 32767 |

## Usage

Hold the camera rotate binding (middle mouse by default) and move the mouse up or down to tilt the camera. The mouse
wheel zooms. On a controller, the right stick tilts and, pressed in, zooms.

From the bg3le console: `Ext.Plugins.GetSettings("LinuxNativeCameraTweaks")` lists the settings, and
`Ext.Plugins.Set("LinuxNativeCameraTweaks", "roll_sensitivity", 1.5)` changes one.

## Build

Needs cmake and the SDL2 headers. `./compile.sh` builds `build/linux_native_camera_tweaks.so` against the host.
`./package.sh` builds the two releases in `dist/`, each its own zip and its own Nexus Mods page: the plugin, against
the Steam Runtime sniper sysroot from bg3le's `tools/build-sniper.sh` so it loads on any distribution, and the MCM
pak (packed with bg3tool).

## Credits and permissions

- Mod created by **Biiinks78**
- Modification and release of bug fixes/improvements allowed, as long as the original creator is credited
- Asset reuse allowed with credit
- Uploading to other sites: **not allowed**
- Conversion for other games: **not allowed**
- Use in paid mods or mods earning donation points: **not allowed**

## Nexus Mods link

[Linux Native Camera Tweaks — Nexus Mods](https://www.nexusmods.com/baldursgate3/mods/23896)
