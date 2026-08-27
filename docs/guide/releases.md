# Releases

The release scripts create a platform-specific archive containing the compiled simulator and its runtime metadata. LVGL and GLFW are linked into the executable.

## Build

Edit the branding and display values in `config/display_config.h`, then run the platform script from the project root.

```text
./scripts/package_release.sh
```

On Windows, run the batch equivalent:

```text
scripts\package_release.bat
```

Unix produces `.tar.gz`; Windows produces `.zip`. Each package has an adjacent SHA-256 file.

## Package contents

| File | Purpose |
|---|---|
| `lvgl-glfw-app` or `lvgl-glfw-app.exe` | Desktop simulator executable |
| `README.md` | Project overview |
| `RELEASE.txt` | Version and runtime expectations |
| `runtime-dependencies.txt` | Host dependency report |
| `VALIDATION.md` | Validation summary |
| `.desktop` launcher and icon asset | Linux branding, when an icon is configured |
| `.ico` asset | Windows branding, when an icon is configured |

## Runtime boundary

The archive includes project code, LVGL, GLFW, and any linked official LVGL content. The host still provides the operating-system runtime, graphics driver, OpenGL 3.3 support, and the active X11, Wayland, Cocoa, or Win32 session.

A release uses the exact LVGL and GLFW revisions pinned by the parent repository. Build separate archives for each operating system and architecture you intend to distribute.

For Linux menu integration, install the generated `.desktop` file and its icon asset together. The executable itself does not require the project source tree or a separate LVGL or GLFW installation.
