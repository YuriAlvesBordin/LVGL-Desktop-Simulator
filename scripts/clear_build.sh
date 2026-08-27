#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
BUILD_DIR="${1:-${BUILD_DIR:-${ROOT_DIR}/build}}"

canonicalize_path() {
    local path="$1"
    if [[ "${path}" != /* ]]; then
        path="${PWD}/${path}"
    fi
    if [[ -d "${path}" ]]; then
        (cd -- "${path}" && pwd -P)
        return
    fi
    local parent_dir
    parent_dir="$(cd -- "$(dirname -- "${path}")" && pwd -P)"
    printf '%s/%s\n' "${parent_dir}" "$(basename -- "${path}")"
}

BUILD_DIR="$(canonicalize_path "${BUILD_DIR}")"

if [[ "${BUILD_DIR}" == "${ROOT_DIR}" || "${BUILD_DIR}" == "/" ]]; then
    echo "Error: refusing to clear the project root or filesystem root." >&2
    exit 2
fi

if [[ ! -e "${BUILD_DIR}" ]]; then
    echo "[clear] build directory does not exist: ${BUILD_DIR}"
    exit 0
fi

rm -rf "${BUILD_DIR}"
echo "[clear] removed ${BUILD_DIR}"
