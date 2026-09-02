# Installation

This project is designed to be easy to clone, configure, build, and run on a desktop machine. LVGL and GLFW are vendored under `external/`, so the repository does not require a package manager for those two libraries.

## Supported environment model

The simulator needs a C compiler, a C++20 compiler, CMake 3.25 or newer, Ninja, an OpenGL development package, and the native window-system development files used by GLFW. The helper scripts additionally need Python 3.9 or newer available as `python3` (Unix) or `python` (Windows).

| Environment | Window system | Additional notes |
|---|---|---|
| Arch Linux | X11 or Wayland | Recommended development environment for this repository |
| Ubuntu or Debian | X11 or Wayland | Use the distribution equivalents of the listed packages |
| Fedora | X11 or Wayland | Use the distribution equivalents of the listed packages |
| macOS | Cocoa | OpenGL 3.3 support depends on the host version and hardware |
| Windows | Win32 | Use a generator and compiler supported by the local CMake installation |

## Arch Linux

Install the compiler toolchain, build tools, OpenGL development files, and the X11 and Wayland dependencies used by the vendored GLFW build.

```text
sudo pacman -Syu --needed base-devel cmake ninja python mesa libglvnd \
  libx11 libxcursor libxinerama libxrandr libxi \
  wayland wayland-protocols libxkbcommon
```

For NVIDIA on Wayland, install the driver package that matches the GPU and kernel, together with the userspace libraries required by the desktop session.

```text
sudo pacman -Syu --needed nvidia-open nvidia-utils egl-wayland libglvnd
```

Do not install the proprietary `nvidia` package and `nvidia-open` as competing driver packages. After a driver update, reboot before diagnosing OpenGL or EGL failures. A healthy installation should report matching kernel module and userspace versions through `nvidia-smi`.

## Ubuntu or Debian

The following package set covers the common Ubuntu and Debian development environment.

```text
sudo apt update
sudo apt install -y build-essential cmake ninja-build python3 pkg-config \
  libgl1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev \
  libwayland-dev libxkbcommon-dev wayland-protocols
```

## Fedora

The following package set covers the common Fedora development environment.

```text
sudo dnf install -y gcc gcc-c++ cmake ninja-build python3 pkgconf-pkg-config \
  mesa-libGL-devel mesa-libEGL-devel \
  libX11-devel libXcursor-devel libXinerama-devel libXrandr-devel libXi-devel \
  wayland-devel libxkbcommon-devel wayland-protocols-devel
```

The project does not depend on system-installed LVGL or GLFW because both are vendored.

## macOS

Homebrew provides the build tools used by the project.

```text
brew install cmake ninja
```

A recent Xcode command-line toolchain is required; it also provides the `python3` interpreter used by the helper scripts. The project requests an OpenGL 3.3 core profile directly. Native OpenGL availability depends on the macOS version and hardware.

## Windows

Install Visual Studio with C++ desktop development, CMake, Python 3.9 or newer, and a generator supported by the local toolchain. The project requests an OpenGL 3.3 core profile directly. The native driver must expose that profile for the application to start.

The workflow scripts are `scripts\live_preview.py` and `scripts\package_release.py`, run with `python` from any shell. The supplied CMake presets use Ninja, so install Ninja when using `cmake --preset debug`; a manual CMake configure can instead use a Visual Studio generator, and the Python scripts select the appropriate configuration directory automatically.

## Python module for custom fonts

Converting fonts from `assets/fonts/` needs the `freetype-py` module (see [Custom fonts](custom-fonts.md)). Distribution packages are preferred where they exist:

```text
sudo apt install python3-freetype      # Ubuntu or Debian
sudo dnf install python3-freetype      # Fedora
pip3 install freetype-py               # macOS, Windows, or any other environment
```

Interpreters managed under PEP 668 (Arch, Homebrew, recent distribution Pythons) reject unmanaged pip installs. In that case install the module in a virtual environment that is active while running CMake, or use `pip3 install --user --break-system-packages freetype-py` if you accept the trade-off. The build picks the first interpreter on the machine that can actually `import freetype`.

## Verify the toolchain

The following commands provide a minimal toolchain check.

On Windows, use the Visual Studio Developer Command Prompt or make sure `cmake`, `python`, and the selected compiler are available on `PATH`.

```text
cmake --version
ninja --version
cc --version
c++ --version
python3 --version
python3 -c "import freetype; print(freetype.version())"
```

On Windows, check `python --version` instead of `python3 --version`.

For a normal graphical session, confirm that the desktop OpenGL stack can expose a 3.3 or newer context. On Wayland, prefer an EGL-oriented diagnostic such as `eglinfo`; `glxinfo` primarily validates the X11 or XWayland GLX path.

## Next step

After installation, follow [First run](first-run.md). Use [Live preview](../development/live-preview.md) for the visible desktop development loop and [Troubleshooting](../troubleshooting/common-issues.md) when diagnosing host graphics issues.
