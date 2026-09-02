#!/usr/bin/env python3
"""Remove a build directory of the LVGL Desktop Simulator.

Usage: clear_build.py [build-dir]

The directory defaults to $BUILD_DIR, then to <project>/build. The project
root, the home directory, and filesystem/drive roots are never removed.
"""

import os
import shutil
import sys
from pathlib import Path

from common import PROJECT_ROOT, error, info


def resolve_target(argv):
    if len(argv) > 1:
        raw = argv[1]
    else:
        raw = os.environ.get("BUILD_DIR") or str(PROJECT_ROOT / "build")
    path = Path(raw).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    return path.resolve()


def is_protected(path):
    if path == Path(path.anchor):
        return True
    if path == PROJECT_ROOT:
        return True
    try:
        if path == Path.home():
            return True
    except RuntimeError:
        pass
    return False


def main(argv):
    target = resolve_target(argv)
    if is_protected(target):
        error(
            "Error: refusing to clear the project root, the home directory, "
            "or a filesystem/drive root."
        )
        return 2
    if not target.exists():
        info("clear", f"build directory does not exist: {target}")
        return 0
    try:
        shutil.rmtree(target)
    except OSError as exc:
        error(f"Error: could not remove {target}: {exc}")
        return 1
    info("clear", f"removed {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
