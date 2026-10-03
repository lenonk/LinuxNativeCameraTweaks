# Changelog

## Linux Native Camera Tweaks - bg3le Plugin

### 1.1 (2026-10-02)

- Scrollable windows (the inventory, an overflowing hotbar) scroll again. The game's UI gets the mouse wheel first,
  and the camera zooms only when nothing in the UI used it.
- Smooth zoom: the camera eases to each new zoom instead of jumping. New settings `smooth_zoom` (on by default) and
  `zoom_smoothing` (how quickly it catches up; lower is smoother).

### 1.0 (2026-10-02)

First release as a bg3le plugin, from Biiinks78's Linux Native Camera Tweaks 1.0.21.

- Loaded by bg3le from its plugins folder; no `LD_PRELOAD` or launch option change. `install.py` installs it.
- Settings for pitch sensitivity, inversion and limits; zoom step, inversion and optional limits; controller pitch
  speed and dead zone. Set in MCM or a settings file, kept between launches.
- Fixed the camera shaking: the game drew its own smoothed pitch and zoom, a frame-time-dependent step off the mod's.
- Fixed a crash on the first mouse event when the camera rotate binding was never changed.
- Works on game version 4.76.31.656.

## Linux Native Camera Tweaks - MCM Settings

### 1.1.0.0 (2026-10-02)

- Smooth zoom and smoothing speed settings, for plugin 1.1.

### 1.0.0.0 (2026-10-02)

- First release: pitch, zoom and controller settings for the plugin in the Mod Configuration Menu. MCM is optional.
