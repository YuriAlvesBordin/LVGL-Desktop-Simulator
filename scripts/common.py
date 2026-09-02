#!/usr/bin/env python3
"""Shared helpers for the LVGL Desktop Simulator helper scripts.

The scripts only use the Python standard library, so they run on every
platform with Python 3.9 or newer.
"""

import hashlib
import re
import shutil
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent

_DEFINE_PATTERN = "#[ \t]*define[ \t]+{macro}[ \t]+(.+?)[ \t]*$"


def read_define(header, macro, default=""):
    """Return the value of the first '#define macro' line in header.

    Strips one pair of surrounding double quotes, matching the behavior of
    the parsers in CMakeLists.txt and the retired shell scripts.
    """
    try:
        text = Path(header).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return default
    match = re.search(
        _DEFINE_PATTERN.format(macro=re.escape(macro)), text, re.MULTILINE
    )
    if match is None:
        return default
    value = match.group(1).strip()
    if len(value) >= 2 and value.startswith('"') and value.endswith('"'):
        value = value[1:-1]
    return value


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def info(tag, message):
    print(f"[{tag}] {message}")


def error(message):
    print(message, file=sys.stderr)


def require_cmake():
    if shutil.which("cmake") is None:
        error("Error: cmake was not found in PATH.")
        sys.exit(127)
