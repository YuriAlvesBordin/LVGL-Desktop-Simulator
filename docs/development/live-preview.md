# Live preview

Live preview rebuilds and relaunches the simulator after project-owned source changes. It is the recommended development loop after the first [Debug](../guide/first-run.md) configure.

## Start

```text
cmake --build build/debug --target live-preview
```

The target runs `scripts/live_preview.py` with Python 3.9 or newer on every platform. The supervisor keeps the previous process alive when a build fails and replaces it only after a successful build.

The preview title is the configured `LVGL_GLFW_WINDOW_TITLE` followed by ` - Preview`. The LVGL screen is recreated on each relaunch; the native window size and position are preserved when the platform permits it.

## What is watched

| Path | Purpose |
|---|---|
| `src/app/` | Application and selected LVGL content |
| `src/integration/` | Desktop bridge changes |
| `config/` | LVGL compile-time configuration |
| `cmake/` | Generated resources such as icons |
| Root CMake files | Targets, presets, and build configuration |
| `scripts/*.py` | Supervisor script changes |

## Display options

The logical LVGL resolution, native window size, presentation mode, screen shape, title, and icon come from `config/display_config.h`. The polling interval comes from `config/project_config.h`. Reconfigure the build after changing either header, then start live preview again.

`LVGL_GLFW_SCREEN_SHAPE` selects rectangle, rounded-corner, or circular presentation using the numeric values defined in `config/display_config.h`. The circular mode is best paired with a square LVGL canvas and preserve-aspect-ratio presentation.

## Window geometry

The supervisor asks the application to close cleanly before relaunching. The executable stores the last client size and position in user state and loads them before creating the next GLFW window.

On Linux, the state file is `XDG_STATE_HOME/lvgl-glfw-window-geometry`, falling back to `HOME/.lvgl-glfw-window-geometry`. Native Wayland compositors control global placement: dimensions can be restored, but exact position is not guaranteed. X11 supports both dimensions and position more directly.
