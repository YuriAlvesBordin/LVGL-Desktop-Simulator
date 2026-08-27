#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
read_define() {
    local header="$1"
    local macro="$2"
    sed -n "s/^[[:space:]]*#[[:space:]]*define[[:space:]]\+${macro}[[:space:]]\+//p" "${header}" | head -n 1 | sed -e 's/^"//' -e 's/"$//'
}

PROJECT_CONFIG="${ROOT_DIR}/config/project_config.h"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}"
INTERVAL="$(read_define "${PROJECT_CONFIG}" LVGL_GLFW_PREVIEW_INTERVAL_SECONDS)"
INTERVAL="${INTERVAL:-0.35}"
APP_BINARY="${BUILD_DIR}/lvgl-glfw-app"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH." >&2
    exit 127
fi
if ! command -v sha256sum >/dev/null 2>&1; then
    echo "Error: sha256sum was not found in PATH." >&2
    exit 127
fi

child_pid=""

stop_app() {
    if [[ -n "${child_pid}" ]] && kill -0 "${child_pid}" 2>/dev/null; then
        echo "[live-preview] stopping application ${child_pid}"
        kill -INT "${child_pid}" 2>/dev/null || true
        for _ in {1..20}; do
            if ! kill -0 "${child_pid}" 2>/dev/null; then
                break
            fi
            sleep 0.05
        done
        if kill -0 "${child_pid}" 2>/dev/null; then
            kill -TERM "${child_pid}" 2>/dev/null || true
        fi
        wait "${child_pid}" 2>/dev/null || true
    fi
    child_pid=""
}

cleanup() {
    stop_app
}
trap cleanup EXIT INT TERM

watch_signature() {
    {
        find "${ROOT_DIR}/src/app" \
             "${ROOT_DIR}/src/integration" \
             "${ROOT_DIR}/config" \
             "${ROOT_DIR}/cmake" \
             -type f -printf '%T@ %s %p\n' 2>/dev/null | sort
        for file in \
            "${ROOT_DIR}/CMakeLists.txt" \
            "${ROOT_DIR}/CMakePresets.json" \
            "${ROOT_DIR}/src/app/CMakeLists.txt" \
            "${ROOT_DIR}/src/integration/CMakeLists.txt"; do
            if [[ -f "${file}" ]]; then
                stat -c '%Y %s %n' "${file}"
            fi
        done
    } | sha256sum | cut -d' ' -f1
}

configure_and_build() {
    local generator_args=()
    if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
        generator_args=(-G Ninja)
    fi

    echo "[live-preview] configuring ${BUILD_TYPE} in ${BUILD_DIR}"
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" "${generator_args[@]}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    echo "[live-preview] building"
    cmake --build "${BUILD_DIR}" --parallel
}

start_app() {
    if [[ ! -x "${APP_BINARY}" ]]; then
        echo "[live-preview] executable not found: ${APP_BINARY}" >&2
        return 1
    fi

    echo "[live-preview] launching ${APP_BINARY}"
    LVGL_GLFW_PREVIEW=1 "${APP_BINARY}" &
    child_pid=$!
}

last_signature=""

while true; do
    current_signature="$(watch_signature)"
    if [[ "${current_signature}" != "${last_signature}" ]]; then
        echo "[live-preview] change detected"
        if configure_and_build; then
            stop_app
            if ! start_app; then
                echo "[live-preview] build succeeded, but the application failed to start" >&2
            fi
            last_signature="${current_signature}"
        else
            echo "[live-preview] build failed; keeping the current application running" >&2
            last_signature="${current_signature}"
        fi
    fi

    if [[ -n "${child_pid}" ]] && ! kill -0 "${child_pid}" 2>/dev/null; then
        wait "${child_pid}" 2>/dev/null || true
        child_pid=""
    fi

    sleep "${INTERVAL}"
done
