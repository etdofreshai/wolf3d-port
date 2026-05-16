#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <build-dir> <port-reference-manifest> [original-reference-manifest] [timeout] [ignore-original-filenames:0|1] [pixel-compare:0|1]"
  exit 1
fi

BUILD_DIR="$1"
PORT_REFERENCE_MANIFEST="$2"
ORIGINAL_MANIFEST="${3:-}"
TIMEOUT_SECONDS="${4:-12}"
IGNORE_ORIG_NAMES="${5:-0}"
PIXEL_COMPARE="${6:-0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKDIR="$(pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ORIGINAL_MANIFEST_NORM=""
PORT_MANIFEST_NORM=""
trap 'rm -f "$ORIGINAL_MANIFEST_NORM" "$PORT_MANIFEST_NORM"' EXIT

export WOLF3D_VERIFY_DEMO="${WOLF3D_VERIFY_DEMO:-0}"
export WOLF3D_SCREENSHOT="${WOLF3D_SCREENSHOT:-1}"
export WOLF3D_SCREENSHOT_STRIDE="${WOLF3D_SCREENSHOT_STRIDE:-18}"
export WOLF3D_SCREENSHOT_COUNT="${WOLF3D_SCREENSHOT_COUNT:-9}"
export WOLF3D_SCREENSHOT_SKIP="${WOLF3D_SCREENSHOT_SKIP:-0}"
export WOLF3D_SCREENSHOT_EXIT="${WOLF3D_SCREENSHOT_EXIT:-1}"
export WOLF3D_SCREENSHOT_EXPECTED="${WOLF3D_SCREENSHOT_EXPECTED:-9}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"

resolve_manifest() {
  local candidate="$1"
  local resolved=""

  if [ -z "$candidate" ]; then
    echo ""
    return 1
  fi

  if [ "${candidate:0:1}" = "/" ]; then
    if [ -f "$candidate" ]; then
      resolved="$candidate"
    fi
  else
    if [ -f "$SCRIPT_DIR/$candidate" ]; then
      resolved="$SCRIPT_DIR/$candidate"
    elif [ -f "$REPO_ROOT/$candidate" ]; then
      resolved="$REPO_ROOT/$candidate"
    elif [ -f "$WORKDIR/$candidate" ]; then
      resolved="$WORKDIR/$candidate"
    fi
  fi

  if [ -n "$resolved" ]; then
    echo "$resolved"
    return 0
  fi

  return 1
}

normalize_manifest_file() {
  local source_file="$1"
  local target_file="$2"
  tr -d '\r' < "$source_file" | sed -e $'1s/^\xEF\xBB\xBF//' > "$target_file"
}

PORT_REFERENCE_MANIFEST="$(resolve_manifest "$PORT_REFERENCE_MANIFEST" || true)"
if [ -z "$PORT_REFERENCE_MANIFEST" ]; then
  echo "verify-parity: unresolved port reference manifest: $2"
  exit 1
fi

if [ -n "$ORIGINAL_MANIFEST" ]; then
  ORIGINAL_MANIFEST="$(resolve_manifest "$ORIGINAL_MANIFEST" || true)"
  if [ -z "$ORIGINAL_MANIFEST" ]; then
    echo "verify-parity: unresolved original reference manifest: $3"
    exit 1
  fi
fi

export WOLF3D_REFERENCE_MANIFEST="$PORT_REFERENCE_MANIFEST"

WOLF3D_VERIFY_TIMEOUT="$TIMEOUT_SECONDS" "$SCRIPT_DIR/verify.sh" "$BUILD_DIR"

MANIFEST_FILE="$(find "$BUILD_DIR" -name wolf3d-autoshot.sha256 | head -n 1)"
if [ -z "$MANIFEST_FILE" ]; then
  echo "verify-parity: could not find generated wolf3d-autoshot.sha256 in $BUILD_DIR"
  exit 1
fi
if [ -n "$ORIGINAL_MANIFEST" ]; then
  ORIGINAL_MANIFEST_NORM="$(mktemp)"
  PORT_MANIFEST_NORM="$(mktemp)"
  normalize_manifest_file "$ORIGINAL_MANIFEST" "$ORIGINAL_MANIFEST_NORM"
  normalize_manifest_file "$MANIFEST_FILE" "$PORT_MANIFEST_NORM"
fi

resolve_manifest_image() {
  local base_dir="$1"
  local name="$2"
  local p

  if [ -z "$name" ]; then
    return 1
  fi

  for p in \
    "$base_dir/$name" \
    "$SCRIPT_DIR/$name" \
    "$REPO_ROOT/$name" \
    "$WORKDIR/$name" \
    "$name"
  do
    if [ -f "$p" ]; then
      echo "$p"
      return 0
    fi
  done

  return 1
}

image_pixel_sha256() {
  local file="$1"
  if ! command -v python3 >/dev/null 2>&1; then
    echo "verify-parity: python3 not found; cannot run pixel compare" >&2
    return 1
  fi
  python3 - "$file" <<'PY'
import sys
import hashlib
try:
    from PIL import Image
except Exception:
    print("verify-parity: Pillow (PIL) not installed; pixel compare unavailable", file=sys.stderr)
    raise

with Image.open(sys.argv[1]) as image:
    image = image.convert("RGBA")
    data = image.tobytes()
    digest = hashlib.sha256(data).hexdigest()
    print(digest)
PY
}

if [ -n "$ORIGINAL_MANIFEST" ]; then
  if [ "$IGNORE_ORIG_NAMES" = "1" ]; then
    if [ "$PIXEL_COMPARE" = "1" ]; then
      : > /tmp/orig.hashes
      : > /tmp/port.hashes
      while read -r hash _name; do
        if [ -z "${_name:-}" ]; then
          continue
        fi
        if ! port_img="$(resolve_manifest_image "$(dirname "$MANIFEST_FILE")" "$_name")"; then
          echo "verify-parity: missing port manifest image: $_name" >&2
          rm -f /tmp/orig.hashes /tmp/port.hashes
          exit 1
        fi
        if ! hash_val="$(image_pixel_sha256 "$port_img" 2>/tmp/verify-parity-image.err)"; then
          cat /tmp/verify-parity-image.err >&2
          rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
          exit 1
        fi
        echo "$hash_val" >> /tmp/port.hashes
      done < "$MANIFEST_FILE"

      while read -r _hash _name; do
        if [ -z "${_name:-}" ]; then
          continue
        fi
        if ! orig_img="$(resolve_manifest_image "$(dirname "$ORIGINAL_MANIFEST")" "$_name")"; then
          echo "verify-parity: missing original manifest image: $_name" >&2
          rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
          exit 1
        fi
        if ! hash_val="$(image_pixel_sha256 "$orig_img" 2>/tmp/verify-parity-image.err)"; then
          cat /tmp/verify-parity-image.err >&2
          rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
          exit 1
        fi
        echo "$hash_val" >> /tmp/orig.hashes
      done < "$ORIGINAL_MANIFEST"

      if ! diff -u /tmp/orig.hashes /tmp/port.hashes >/tmp/verify-parity.diff; then
        echo "verify-parity: original baseline mismatch (pixel compare)"
        cat /tmp/verify-parity.diff
        rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity.diff /tmp/verify-parity-image.err
        exit 1
      fi
      rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity.diff /tmp/verify-parity-image.err

    else
      awk '{print $1}' "$ORIGINAL_MANIFEST_NORM" > /tmp/orig.hashes
      awk '{print $1}' "$PORT_MANIFEST_NORM" > /tmp/port.hashes
      if ! diff -u /tmp/orig.hashes /tmp/port.hashes >/tmp/verify-parity.diff; then
        echo "verify-parity: original baseline mismatch (hash-only compare)"
        cat /tmp/verify-parity.diff
        rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity.diff
        exit 1
      fi
      rm -f /tmp/orig.hashes /tmp/port.hashes
    fi
  elif [ "$PIXEL_COMPARE" = "1" ]; then
    : > /tmp/orig.hashes
    : > /tmp/port.hashes
    while read -r hash _name; do
      if [ -z "${_name:-}" ]; then
        continue
      fi
      if ! port_img="$(resolve_manifest_image "$(dirname "$MANIFEST_FILE")" "$_name")"; then
        echo "verify-parity: missing port manifest image: $_name" >&2
        rm -f /tmp/orig.hashes /tmp/port.hashes
        exit 1
      fi
      if ! hash_val="$(image_pixel_sha256 "$port_img" 2>/tmp/verify-parity-image.err)"; then
        cat /tmp/verify-parity-image.err >&2
        rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
        exit 1
      fi
      echo "$hash_val" >> /tmp/port.hashes
    done < "$MANIFEST_FILE"

    while read -r _hash _name; do
      if [ -z "${_name:-}" ]; then
        continue
      fi
      if ! orig_img="$(resolve_manifest_image "$(dirname "$ORIGINAL_MANIFEST")" "$_name")"; then
        echo "verify-parity: missing original manifest image: $_name" >&2
        rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
        exit 1
      fi
      if ! hash_val="$(image_pixel_sha256 "$orig_img" 2>/tmp/verify-parity-image.err)"; then
        cat /tmp/verify-parity-image.err >&2
        rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity-image.err
        exit 1
      fi
      echo "$hash_val" >> /tmp/orig.hashes
    done < "$ORIGINAL_MANIFEST"

    if ! diff -u /tmp/orig.hashes /tmp/port.hashes >/tmp/verify-parity.diff; then
      echo "verify-parity: original baseline mismatch (pixel compare)"
      cat /tmp/verify-parity.diff
      rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity.diff /tmp/verify-parity-image.err
      exit 1
    fi
    rm -f /tmp/orig.hashes /tmp/port.hashes /tmp/verify-parity.diff /tmp/verify-parity-image.err

  else
    if ! diff -u "$ORIGINAL_MANIFEST_NORM" "$PORT_MANIFEST_NORM" >/tmp/verify-parity.diff; then
      echo "verify-parity: original baseline mismatch"
      cat /tmp/verify-parity.diff
      rm -f /tmp/verify-parity.diff
      exit 1
    fi
  fi
  if [ -f /tmp/verify-parity.diff ]; then
    rm -f /tmp/verify-parity.diff
  fi
  if [ -f /tmp/verify-parity-image.err ]; then
    rm -f /tmp/verify-parity-image.err
  fi
  echo "verify-parity: original baseline matched: $ORIGINAL_MANIFEST"
fi

echo "verify-parity: complete"
