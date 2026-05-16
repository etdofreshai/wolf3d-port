#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <capture-dir> [manifest-path] [pattern] [normalize:0|1]"
  exit 1
fi

CAPTURE_DIR="$1"
MANIFEST_PATH="${2:-scripts/wolf3d-autoshot-original.sha256}"
PATTERN="${3:-autoshot_*.bmp}"
NORMALIZE="${4:-0}"
START_INDEX="${5:-0}"
TARGET_EXT="${6:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKDIR="$(pwd)"

if [ -z "${MANIFEST_PATH}" ]; then
  MANIFEST_PATH="scripts/wolf3d-autoshot-original.sha256"
fi

if [ "${MANIFEST_PATH:0:1}" != "/" ]; then
  MANIFEST_PATH="$REPO_ROOT/$MANIFEST_PATH"
fi

if [ ! -d "$CAPTURE_DIR" ]; then
  echo "capture directory not found: $CAPTURE_DIR" >&2
  exit 1
fi

shopt -s nullglob
mapfile -t SHOTS < <(cd "$CAPTURE_DIR" && find . -maxdepth 1 -type f -name "$PATTERN" | sort)
if [ "${#SHOTS[@]}" -eq 0 ]; then
  echo "no matching files found in $CAPTURE_DIR (pattern=$PATTERN)" >&2
  exit 1
fi

mkdir -p "$(dirname "$MANIFEST_PATH")"
: > "$MANIFEST_PATH"

for idx in "${!SHOTS[@]}"; do
  shot="${SHOTS[$idx]}"
  shot_name="${shot#./}"
  manifest_name="$shot_name"

  if [ "$NORMALIZE" = "1" ]; then
    ext="${shot_name##*.}"
    if [ -n "$TARGET_EXT" ]; then
      ext="$TARGET_EXT"
    fi
    manifest_name="$(printf 'autoshot_%03d.%s' "$((idx + START_INDEX))" "$ext")"
  fi

  full_path="$CAPTURE_DIR/$shot_name"
  hash="$(sha256sum "$full_path" | awk '{print $1}')"
  printf '%s  %s\n' "$hash" "$manifest_name" | tee -a "$MANIFEST_PATH"
done

echo "Wrote manifest from capture directory: $MANIFEST_PATH"
echo "Captured files: ${#SHOTS[@]}"
echo "Pattern: $PATTERN"
