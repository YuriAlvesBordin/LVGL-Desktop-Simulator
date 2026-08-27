# Selecting LVGL content

The application layer is intentionally reduced to one decision: which official LVGL example or demo should be initialized. The decision lives in `src/app/Application.c`.

## Official examples

LVGL examples are grouped under `external/lvgl/examples/`. Their public declarations are organized by category headers such as `get_started/lv_example_get_started.h`, `widgets/lv_example_widgets.h`, `anim/lv_example_anim.h`, `styles/lv_example_style.h`, and `layouts/lv_example_layout.h`.

Examples are useful when a focused widget, layout, animation, event, or API behavior needs to be inspected in isolation. The application only needs to include the appropriate public header and call one declared example function.

## Official demos

LVGL demos are grouped under `external/lvgl/demos/`. The default project configuration enables the widgets demo through `LV_USE_DEMO_WIDGETS` and exposes its public header through the `lvgl::demos` target.

The current default is `lv_demo_widgets()`, declared by `widgets/lv_demo_widgets.h`. This demo provides a richer showcase of LVGL widgets and layouts than a single focused example.

## Selection rules

Select one primary screen-producing function for each run. Most examples and demos create objects on the active LVGL screen, so calling several unrelated screen-producing functions in sequence usually produces an overlapping showcase rather than a meaningful comparison.

Keep `lvgl.h` before the selected example or demo header. This ensures that LVGL configuration macros and public types are available when the category header is processed.

Use the existing configuration in `config/lv_conf.h` as the source of truth for enabled widgets, fonts, layouts, demos, and renderer features. If a selected content unit requires a disabled feature, update the configuration and rebuild instead of modifying the vendored LVGL source.

## Where the build comes from

The root CMake configuration reads `LV_BUILD_EXAMPLES` and `LV_BUILD_DEMOS` from `config/lv_conf.h`. The application target links `lvgl::examples` and `lvgl::demos` through `src/app/CMakeLists.txt`.

No example source file should be copied into `src/app`. The application directory is the selection point, not a fork of the LVGL content tree.

## Switching content

To switch content, edit only `src/app/Application.c`, replace the selected declaration header and function call, save the file, and use the live preview workflow. The supervisor detects the change, rebuilds the affected targets, and relaunches the simulator after a successful build.

## Finding available functions

The vendored LVGL headers are the authoritative index for public example and demo declarations. The category headers under `external/lvgl/examples/` and `external/lvgl/demos/` can be searched without relying on a second catalog maintained by this project.

## Recommended progression

| Goal | Starting point |
|---|---|
| Verify that the simulator is alive | A `get_started` example |
| Inspect a single widget | A function under `widgets/lv_example_widgets.h` |
| Explore the complete widget set | `lv_demo_widgets()` |
| Inspect input-oriented behavior | An event, keyboard, textarea, or keypad-oriented example or demo |
| Compare layouts | A flex or grid example under `layouts/` |
| Inspect animation APIs | A function under `anim/` |
| Validate rendering features | A rendering or benchmark demo, subject to the enabled LVGL configuration |
