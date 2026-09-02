#!/usr/bin/env python3
"""Build and package a standalone release of the LVGL desktop simulator.

Unix produces dist/<package>.tar.gz and Windows produces dist/<package>.zip,
each with an adjacent .sha256 checksum file.
"""

import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

from common import PROJECT_ROOT, error, info, read_define, require_cmake, sha256_file

TAG = "release"
TARGET_NAME = "lvgl_glfw_app"
BINARY_NAME = "lvgl-glfw-app"


def detect_platform():
    return platform.system().lower()


def detect_arch():
    machine = (platform.machine() or "unknown").lower()
    if sys.platform == "win32":
        return "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    return "x86_64" if machine in ("amd64", "x64") else machine


def find_app_binary(build_dir):
    candidates = []
    if sys.platform == "win32":
        candidates += [
            build_dir / f"{BINARY_NAME}.exe",
            build_dir / "Release" / f"{BINARY_NAME}.exe",
        ]
    else:
        candidates.append(build_dir / BINARY_NAME)
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def stage_icon(package_dir):
    display_config = PROJECT_ROOT / "config" / "display_config.h"
    icon_value = read_define(display_config, "LVGL_GLFW_ICON_PATH")
    if not icon_value:
        return None
    icon_path = Path(icon_value)
    if not icon_path.is_absolute():
        icon_path = PROJECT_ROOT / icon_path
    if not icon_path.is_file():
        return None
    staged = package_dir / f"lvgl-glfw-app-icon{icon_path.suffix}"
    shutil.copy2(icon_path, staged)
    return staged


def write_release_notes(package_dir, version, release_platform, arch):
    if release_platform == "windows":
        runtime_line = (
            "The host still needs a compatible OpenGL 3.3 driver and native "
            "Windows runtime components."
        )
        icon_line = (
            "The configured window title and optional ICO icon are included "
            "in the executable."
        )
    else:
        runtime_line = (
            "The host still needs a compatible OpenGL 3.3 driver and native "
            "window-system libraries."
        )
        icon_line = (
            "The configured window title and optional BMP icon are included "
            "in the executable."
        )
    (package_dir / "RELEASE.txt").write_text(
        "\n".join(
            [
                f"LVGL Desktop Simulator release {version}",
                f"Platform: {release_platform}",
                f"Architecture: {arch}",
                "LVGL and GLFW are linked into the executable from Git submodules.",
                runtime_line,
                icon_line,
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def write_runtime_dependencies(package_dir, binary, release_platform):
    report = package_dir / "runtime-dependencies.txt"
    if release_platform != "windows" and shutil.which("ldd"):
        result = subprocess.run(
            ["ldd", str(binary)], capture_output=True, text=True, check=False
        )
        report.write_text(result.stdout, encoding="utf-8")
    else:
        report.write_text(
            "Windows host dependencies are provided by the operating system "
            "and graphics driver.\n",
            encoding="utf-8",
        )


def write_archive(staging_dir, package_dir, package_name, archive_path):
    if archive_path.exists():
        archive_path.unlink()
    if sys.platform == "win32":
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as bundle:
            for path in sorted(package_dir.rglob("*")):
                if path.is_file():
                    bundle.write(path, path.relative_to(staging_dir).as_posix())
    else:
        with tarfile.open(archive_path, "w:gz") as bundle:
            bundle.add(package_dir, arcname=package_name)


def main():
    require_cmake()
    release_platform = (
        os.environ.get("LVGL_GLFW_RELEASE_PLATFORM") or detect_platform()
    )
    arch = os.environ.get("LVGL_GLFW_RELEASE_ARCH") or detect_arch()
    version = (
        os.environ.get("LVGL_GLFW_RELEASE_VERSION")
        or read_define(
            PROJECT_ROOT / "config" / "project_config.h",
            "LVGL_GLFW_PROJECT_VERSION",
            "0.0.0",
        )
        or "0.0.0"
    )
    build_dir = Path(
        os.environ.get("BUILD_DIR") or (PROJECT_ROOT / "build" / "release")
    )
    dist_dir = Path(os.environ.get("DIST_DIR") or (PROJECT_ROOT / "dist"))
    package_name = f"lvgl-desktop-simulator-{version}-{release_platform}-{arch}"
    archive_extension = ".zip" if sys.platform == "win32" else ".tar.gz"
    archive_path = dist_dir / f"{package_name}{archive_extension}"

    dist_dir.mkdir(parents=True, exist_ok=True)

    configure = ["cmake", "-S", str(PROJECT_ROOT), "-B", str(build_dir)]
    if shutil.which("ninja"):
        configure += ["-G", "Ninja"]
    configure += ["-DCMAKE_BUILD_TYPE=Release"]
    info(TAG, f"configuring {build_dir}")
    status = subprocess.call(configure)
    if status != 0:
        sys.exit(status)

    info(TAG, f"building {TARGET_NAME}")
    build = ["cmake", "--build", str(build_dir), "--target", TARGET_NAME, "--parallel"]
    if sys.platform == "win32":
        build += ["--config", "Release"]
    status = subprocess.call(build)
    if status != 0:
        sys.exit(status)

    binary = find_app_binary(build_dir)
    if binary is None:
        error(f"Error: release executable was not produced in {build_dir}.")
        sys.exit(1)

    with tempfile.TemporaryDirectory(prefix="lvgl-release-") as staging_dir:
        package_dir = Path(staging_dir) / package_name
        package_dir.mkdir()
        shutil.copy2(binary, package_dir / binary.name)
        shutil.copy2(PROJECT_ROOT / "README.md", package_dir / "README.md")
        for optional in ("VALIDATION.md", "LICENSE"):
            source = PROJECT_ROOT / optional
            if source.is_file():
                shutil.copy2(source, package_dir / optional)

        staged_icon = stage_icon(package_dir)
        if release_platform == "linux" and staged_icon is not None:
            window_title = (
                read_define(
                    PROJECT_ROOT / "config" / "display_config.h",
                    "LVGL_GLFW_WINDOW_TITLE",
                    "LVGL Desktop Simulator",
                )
                or "LVGL Desktop Simulator"
            )
            (package_dir / "lvgl-glfw-app.desktop").write_text(
                "[Desktop Entry]\n"
                f"Name={window_title}\n"
                "Exec=lvgl-glfw-app\n"
                "Icon=lvgl-glfw-app-icon\n"
                "Terminal=false\n"
                "Categories=Development;\n",
                encoding="utf-8",
            )

        write_release_notes(package_dir, version, release_platform, arch)
        write_runtime_dependencies(package_dir, binary, release_platform)
        write_archive(staging_dir, package_dir, package_name, archive_path)

    checksum_path = archive_path.with_name(archive_path.name + ".sha256")
    checksum_path.write_text(
        f"{sha256_file(archive_path)}  {archive_path.name}\n", encoding="utf-8"
    )
    info(TAG, f"archive: {archive_path}")
    info(TAG, f"checksum: {checksum_path}")


if __name__ == "__main__":
    main()
