# First run

This is the shortest path from a checkout to a running simulator.

## Recommended workflow

```text
cmake --preset debug
cmake --build --preset debug-build
./build/debug/lvgl-glfw-app
```

The application entry point is `src/app/Application.c`. The default call shows a centered button with the label "Live preview". To run the LVGL widgets demo, edit `src/app/Application.c` and uncomment `lv_demo_widgets()` while commenting out the button code.

## Display configuration

The LVGL canvas and the native window are independent. Edit `config/display_config.h` when the default `800×480` setup is not appropriate. The same header contains the presentation mode, screen shape, corner radius, title, icon path, VSync interval, and maximum FPS.

Set `LVGL_GLFW_SCREEN_SHAPE` to `2` with equal LVGL width and height for a circular display, and set `LVGL_GLFW_PRESENTATION_MODE` to `1` when preserving the canvas aspect ratio or to `2` when the canvas should keep its true resolution at one canvas pixel per framebuffer pixel. Reconfigure with the Debug preset after changing the header, then build again. The complete configuration map is in [CMake and presets](../reference/cmake.md).

## Select another LVGL screen

Edit `src/app/Application.c` and replace the selected official LVGL example or demo. Keep `lvgl.h` before the selected category header. The next live-preview rebuild or normal launch uses the new screen.

The available declarations are in `external/lvgl/examples/` and `external/lvgl/demos/`. The selection rules are documented in [Selecting LVGL content](selecting-lvgl-content.md).

## Continue

Use [Live preview](../development/live-preview.md) for automatic rebuilds. Use [CMake and presets](../reference/cmake.md) for build targets, cleanup, and advanced configuration. If the window does not open, follow [Common issues](../troubleshooting/common-issues.md).
