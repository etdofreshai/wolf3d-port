#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${1:-build}"
SDL3_FETCH="${WOLF3D_FETCH_SDL3:-ON}"
GENERATOR="${2:-Unix Makefiles}"

if [ ! -d "${BUILD_DIR}" ]; then
  mkdir -p "${BUILD_DIR}"
fi

cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" -DWOLF3D_FETCH_SDL3="${SDL3_FETCH}"
cmake --build "${BUILD_DIR}" -j "${CI:-$(nproc 2>/dev/null || echo 4)}"
