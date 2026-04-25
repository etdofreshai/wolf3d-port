#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
deps_dir="${WOLF3D_DEPS_DIR:-$repo_root/.deps}"
src_dir="$deps_dir/SDL-src"
build_dir="$deps_dir/SDL-build"
prefix_dir="${SDL3_PREFIX:-$deps_dir/SDL3}"
repo_url="${SDL3_REPO_URL:-https://github.com/libsdl-org/SDL.git}"
ref="${SDL3_REF:-main}"

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required tool: $1" >&2
    return 1
  fi
}

need git
need cmake
need cc

mkdir -p "$deps_dir"

if [ ! -d "$src_dir/.git" ]; then
  echo "Cloning SDL3 from $repo_url ($ref) into $src_dir"
  git clone --depth 1 --branch "$ref" "$repo_url" "$src_dir"
else
  echo "Updating existing SDL source in $src_dir"
  git -C "$src_dir" fetch --depth 1 origin "$ref"
  git -C "$src_dir" checkout FETCH_HEAD
fi

cmake -S "$src_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix_dir" \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DSDL_SHARED=ON \
  -DSDL_STATIC=ON

cmake --build "$build_dir" --target install --parallel "${SDL3_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

pc_dir="$prefix_dir/lib/pkgconfig"
if [ ! -f "$pc_dir/sdl3.pc" ]; then
  pc_dir="$prefix_dir/lib64/pkgconfig"
fi

echo "SDL3 installed under: $prefix_dir"
echo "Use it with:"
echo "  export PKG_CONFIG_PATH='$pc_dir'\${PKG_CONFIG_PATH:+:\$PKG_CONFIG_PATH}"
echo "  cd source/modern-c-sdl3 && make test-sdl3"
