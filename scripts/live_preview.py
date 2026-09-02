#!/usr/bin/env python3
"""Automatic rebuild-and-relaunch supervisor for the LVGL desktop simulator.

Watches the application sources, reconfigures and rebuilds with CMake on
change, then restarts the desktop app. Runs on every platform with Python
3.9 or newer.
"""

import os
import shutil
import signal
import subprocess
import sys
import time
from hashlib import sha256
from pathlib import Path

from common import PROJECT_ROOT, error, info, read_define, require_cmake

TAG = "live-preview"
WATCHED_DIRECTORIES = ("src/app", "src/integration", "config", "cmake")
WATCHED_FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "src/app/CMakeLists.txt",
    "src/integration/CMakeLists.txt",
)


def read_interval(project_config):
    raw = read_define(project_config, "LVGL_GLFW_PREVIEW_INTERVAL_SECONDS", "0.35")
    try:
        interval = float(raw)
    except (TypeError, ValueError):
        interval = 0.35
    return max(0.05, interval)


def app_candidates(build_dir, build_type):
    if sys.platform == "win32":
        return [
            build_dir / "lvgl-glfw-app.exe",
            build_dir / build_type / "lvgl-glfw-app.exe",
        ]
    return [build_dir / "lvgl-glfw-app"]


def is_executable_file(path):
    return path.is_file() and (os.name == "nt" or os.access(path, os.X_OK))


def watched_paths():
    paths = []
    for relative in WATCHED_DIRECTORIES:
        directory = PROJECT_ROOT / relative
        if not directory.is_dir():
            continue
        for base, dirnames, filenames in os.walk(directory):
            dirnames.sort()
            for name in sorted(filenames):
                paths.append(Path(base) / name)
    extra = list(WATCHED_FILES)
    scripts_dir = PROJECT_ROOT / "scripts"
    if scripts_dir.is_dir():
        extra += sorted(f"scripts/{entry.name}" for entry in scripts_dir.glob("*.py"))
    for relative in extra:
        candidate = PROJECT_ROOT / relative
        if candidate.is_file():
            paths.append(candidate)
    return paths


def watch_signature():
    records = []
    for path in watched_paths():
        try:
            stat = path.stat()
        except OSError:
            continue
        # Only string/stat operations here: Path-vs-Path comparison in the
        # hot loop is fragile (pathlib internals blew up once mid-run).
        records.append(f"{path}|{stat.st_size}|{stat.st_mtime_ns}")
    records.sort()
    return sha256("\n".join(records).encode("utf-8")).hexdigest()


def build_project(build_dir, build_type):
    configure = ["cmake", "-S", str(PROJECT_ROOT), "-B", str(build_dir)]
    if not (build_dir / "CMakeCache.txt").is_file() and shutil.which("ninja"):
        configure += ["-G", "Ninja"]
    configure += [f"-DCMAKE_BUILD_TYPE={build_type}"]
    info(TAG, f"configuring {build_type} in {build_dir}")
    if subprocess.call(configure) != 0:
        return False
    build = ["cmake", "--build", str(build_dir), "--parallel"]
    if sys.platform == "win32":
        build += ["--config", build_type]
    info(TAG, "building")
    return subprocess.call(build) == 0


def start_app(build_dir, build_type):
    candidates = app_candidates(build_dir, build_type)
    for candidate in candidates:
        if is_executable_file(candidate):
            info(TAG, f"launching {candidate}")
            env = dict(os.environ)
            env["LVGL_GLFW_PREVIEW"] = "1"
            return subprocess.Popen([str(candidate)], cwd=str(build_dir), env=env)
    error(f"[{TAG}] executable not found: {candidates[0]}")
    return None


def close_windows_of(pid):
    """Post WM_CLOSE to the visible top-level windows owned by pid (Windows)."""
    try:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.windll.user32
        wm_close = 0x0010
        posted = []

        enum_proc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

        def on_window(hwnd, _lparam):
            owner = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == pid and user32.IsWindowVisible(hwnd):
                user32.PostMessageW(hwnd, wm_close, 0, 0)
                posted.append(hwnd)
            return True

        user32.EnumWindows(enum_proc(on_window), 0)
        return bool(posted)
    except Exception:
        return False


def stop_app(process):
    if process is None or process.poll() is not None:
        return
    info(TAG, f"stopping application {process.pid}")
    if sys.platform == "win32":
        # There are no POSIX signals here; the app closes its GLFW window on WM_CLOSE.
        close_windows_of(process.pid)
        deadline = time.monotonic() + 2.0
        while process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
    else:
        try:
            os.kill(process.pid, signal.SIGINT)
        except ProcessLookupError:
            pass
        for _ in range(20):
            if process.poll() is not None:
                break
            time.sleep(0.05)
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def main():
    require_cmake()
    project_config = PROJECT_ROOT / "config" / "project_config.h"
    build_dir = Path(os.environ.get("BUILD_DIR") or (PROJECT_ROOT / "build"))
    build_type = os.environ.get("CMAKE_BUILD_TYPE") or "Debug"
    interval = read_interval(project_config)

    def request_shutdown(_signum, _frame):
        raise SystemExit(0)

    signal.signal(signal.SIGTERM, request_shutdown)

    process = None
    last_signature = ""
    try:
        while True:
            try:
                current_signature = watch_signature()
            except Exception:
                # A transient filesystem error must never kill the supervisor.
                error(f"[{TAG}] signature scan failed; retrying")
                time.sleep(interval)
                continue
            if current_signature != last_signature:
                info(TAG, "change detected")
                if build_project(build_dir, build_type):
                    stop_app(process)
                    process = start_app(build_dir, build_type)
                    if process is None:
                        error(
                            f"[{TAG}] build succeeded, but the application "
                            "failed to start"
                        )
                else:
                    error(
                        f"[{TAG}] build failed; keeping the current "
                        "application running"
                    )
                last_signature = current_signature
            if process is not None and process.poll() is not None:
                code = process.returncode
                process.wait()
                process = None
                if code != 0:
                    error(f"[{TAG}] application exited with code {code}")
                else:
                    info(TAG, "application exited")
            time.sleep(interval)
    finally:
        stop_app(process)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
