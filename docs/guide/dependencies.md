# Dependencies

LVGL and GLFW are tracked as Git submodules. The parent repository pins the exact commits used by each build.

| Dependency | Path |
|---|---|
| LVGL | `external/lvgl` |
| GLFW | `external/glfw` |

## Clone

Use one recursive clone for a complete checkout:

```text
git clone --recurse-submodules https://github.com/YuriAlvesBordin/LVGL-Desktop-Simulator
```

If the repository was already cloned without its submodules, run the recovery command from the project root:

```text
git submodule update --init --recursive
```

No separate LVGL or GLFW installation is required. The CMake project builds the pinned submodule sources directly.

## Python tooling

The helper scripts and the custom font converter run on the standard library of the available Python 3 interpreter, with one exception: converting fonts from `assets/fonts/` needs the `freetype-py` module, a pip-installable binding over FreeType. See [Custom fonts](custom-fonts.md) and [Installation](installation.md).

## Repository boundary

The parent stores submodule gitlinks in `.gitmodules`; it does not copy dependency history into the project. Do not edit `external/lvgl` or `external/glfw` for application work. Change `src/app/Application.c` for screen selection and update a submodule revision only as an intentional dependency change.

The current submodule revisions can be inspected with `git submodule status`. A clean release should keep the parent commit and both submodule gitlinks recorded together.
