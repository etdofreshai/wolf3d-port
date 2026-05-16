#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${1:-build-debug}"
DEMO="${2:-0}"
TIMEOUT_SECONDS="${3:-12}"
REFERENCE_PATH="${4:-}"

if [ ! -d "$BUILD_DIR" ]; then
  echo "build directory not found: $BUILD_DIR" >&2
  exit 1
fi

if [ "$DEMO" -lt 0 ] || [ "$DEMO" -gt 3 ]; then
  echo "Demo must be between 0 and 3" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "$REFERENCE_PATH" ]; then
  REFERENCE_PATH="scripts/wolf3d-autoshot-demo${DEMO}-reference.sha256"
fi

export WOLF3D_VERIFY_DEMO="$DEMO"
export WOLF3D_SCREENSHOT=1
export WOLF3D_SCREENSHOT_STRIDE=18
export WOLF3D_SCREENSHOT_COUNT=9
export WOLF3D_SCREENSHOT_EXIT=1
export WOLF3D_SCREENSHOT_EXPECTED=9
export WOLF3D_REFERENCE_MANIFEST=""

WOLF3D_VERIFY_TIMEOUT="$TIMEOUT_SECONDS" \
  "$SCRIPT_DIR/verify.sh" "$BUILD_DIR"

MANIFEST="$(find "$BUILD_DIR" -name wolf3d-autoshot.sha256 | sort | head -n 1)"
if [ -z "$MANIFEST" ]; then
  echo "could not find generated wolf3d-autoshot.sha256 in $BUILD_DIR" >&2
  exit 1
fi

mkdir -p "$(dirname "$REFERENCE_PATH")"
cp "$MANIFEST" "$REFERENCE_PATH"
echo "wrote reference manifest: $REFERENCE_PATH"
echo "source manifest: $MANIFEST"
