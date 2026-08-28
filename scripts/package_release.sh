#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
read_define() {
    local header="$1"
    local macro="$2"
    sed -n "s/^[[:space:]]*#[[:space:]]*define[[:space:]]\+${macro}[[:space:]]\+//p" "${header}" | head -n 1 | sed -e 's/^"//' -e 's/"$//'
}

PROJECT_CONFIG="${ROOT_DIR}/config/project_config.h"
DISPLAY_CONFIG="${ROOT_DIR}/config/display_config.h"
VERSION="$(read_define "${PROJECT_CONFIG}" LVGL_GLFW_PROJECT_VERSION)"
VERSION="${VERSION:-0.0.0}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/release}"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist}"
PLATFORM="${LVGL_GLFW_RELEASE_PLATFORM:-$(uname -s | tr '[:upper:]' '[:lower:]')}"
ARCHITECTURE="${LVGL_GLFW_RELEASE_ARCH:-$(uname -m)}"
PACKAGE_NAME="lvgl-desktop-simulator-${VERSION}-${PLATFORM}-${ARCHITECTURE}"
STAGING_DIR="$(mktemp -d)"
PACKAGE_DIR="${STAGING_DIR}/${PACKAGE_NAME}"
ARCHIVE_PATH="${DIST_DIR}/${PACKAGE_NAME}.tar.gz"

cleanup() {
    rm -rf "${STAGING_DIR}"
}
trap cleanup EXIT INT TERM

command -v cmake >/dev/null 2>&1 || { echo "Error: cmake was not found in PATH." >&2; exit 127; }
command -v tar >/dev/null 2>&1 || { echo "Error: tar was not found in PATH." >&2; exit 127; }
if command -v sha256sum >/dev/null 2>&1; then
    SHA256_COMMAND=(sha256sum)
elif command -v shasum >/dev/null 2>&1; then
    SHA256_COMMAND=(shasum -a 256)
else
    echo "Error: sha256sum or shasum was not found in PATH." >&2
    exit 127
fi

mkdir -p "${DIST_DIR}" "${PACKAGE_DIR}"

echo "[release] configuring ${BUILD_DIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

echo "[release] building lvgl-glfw-app"
cmake --build "${BUILD_DIR}" --target lvgl_glfw_app --parallel

APP_BINARY="${BUILD_DIR}/lvgl-glfw-app"
if [[ ! -x "${APP_BINARY}" ]]; then
    echo "Error: release executable was not produced at ${APP_BINARY}." >&2
    exit 1
fi

cp "${APP_BINARY}" "${PACKAGE_DIR}/lvgl-glfw-app"
cp "${ROOT_DIR}/README.md" "${PACKAGE_DIR}/README.md"
cp "${ROOT_DIR}/VALIDATION.md" "${PACKAGE_DIR}/VALIDATION.md" 2>/dev/null || true
cp "${ROOT_DIR}/LICENSE" "${PACKAGE_DIR}/LICENSE" 2>/dev/null || true

ICON_PATH="$(read_define "${DISPLAY_CONFIG}" LVGL_GLFW_ICON_PATH)"
WINDOW_TITLE="$(read_define "${DISPLAY_CONFIG}" LVGL_GLFW_WINDOW_TITLE)"
if [[ -n "${ICON_PATH}" && "${ICON_PATH}" != /* ]]; then
    ICON_PATH="${ROOT_DIR}/${ICON_PATH}"
fi
if [[ -n "${ICON_PATH}" && -f "${ICON_PATH}" ]]; then
    cp "${ICON_PATH}" "${PACKAGE_DIR}/lvgl-glfw-app-icon.bmp"
    cat > "${PACKAGE_DIR}/lvgl-glfw-app.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${WINDOW_TITLE:-LVGL Desktop Simulator}
Exec=lvgl-glfw-app
Icon=lvgl-glfw-app-icon
Terminal=false
Categories=Development;
EOF
fi

printf '%s\n' \
    "LVGL Desktop Simulator release ${VERSION}" \
    "Platform: ${PLATFORM}" \
    "Architecture: ${ARCHITECTURE}" \
    "LVGL and GLFW are linked into the executable from Git submodules." \
    "The host still needs a compatible OpenGL 3.3 driver and native window-system libraries." \
    "The configured window title and optional BMP icon are included in the executable." \
    > "${PACKAGE_DIR}/RELEASE.txt"

if command -v ldd >/dev/null 2>&1; then
    ldd "${APP_BINARY}" > "${PACKAGE_DIR}/runtime-dependencies.txt" || true
fi

rm -f "${ARCHIVE_PATH}"
tar -C "${STAGING_DIR}" -czf "${ARCHIVE_PATH}" "${PACKAGE_NAME}"
"${SHA256_COMMAND[@]}" "${ARCHIVE_PATH}" > "${ARCHIVE_PATH}.sha256"

printf '%s\n' "[release] archive: ${ARCHIVE_PATH}" "[release] checksum: ${ARCHIVE_PATH}.sha256"
