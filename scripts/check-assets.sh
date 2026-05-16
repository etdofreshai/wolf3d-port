#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST_PATH="${1:-$ROOT_DIR/scripts/assets-manifest.sha256}"
MODE="${2:-check}"   # check (default) | write

if [[ ! -d "$ROOT_DIR/original" ]]; then
  echo "check-assets: original/ directory not found at $ROOT_DIR/original" >&2
  exit 1
fi
if [[ ! -d "$ROOT_DIR/assets" ]]; then
  echo "check-assets: assets/ directory not found at $ROOT_DIR/assets" >&2
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  HASHER="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  HASHER="shasum -a 256"
else
  echo "check-assets: sha256sum or shasum is required" >&2
  exit 1
fi

compute_manifest() {
  local out_file="$1"
  : > "$out_file"
  (
    cd "$ROOT_DIR"
    find original assets -type f | LC_ALL=C sort | while IFS= read -r file; do
      local hash
      if [[ "$HASHER" == "sha256sum" ]]; then
        hash="$(sha256sum "$file" | awk '{print $1}')"
      else
        hash="$(shasum -a 256 "$file" | awk '{print $1}')"
      fi
      printf '%s  %s\n' "$hash" "$file"
    done
  ) >> "$out_file"
}

TMP_MANIFEST="$(mktemp)"
TMP_MANIFEST_NORM="${TMP_MANIFEST}.norm"
MANIFEST_NORM="$(mktemp)"
trap 'rm -f "$TMP_MANIFEST" "$TMP_MANIFEST_NORM" "$MANIFEST_NORM"' EXIT
compute_manifest "$TMP_MANIFEST"

case "$MODE" in
  check)
    if [[ ! -f "$MANIFEST_PATH" ]]; then
      echo "check-assets: manifest not found: $MANIFEST_PATH"
      echo "check-assets: initialize with: scripts/check-assets.sh \"$MANIFEST_PATH\" write"
      exit 1
    fi
    LC_ALL=C sort "$TMP_MANIFEST" > "$TMP_MANIFEST_NORM"
    tr -d '\r' < "$MANIFEST_PATH" | sed -e $'1s/\xEF\xBB\xBF//' | LC_ALL=C sort > "$MANIFEST_NORM"
    if diff -u "$MANIFEST_NORM" "$TMP_MANIFEST_NORM" >/tmp/check-assets.diff; then
      echo "check-assets: PASS (original and assets match manifest)"
    else
      echo "check-assets: FAIL (asset fingerprints changed)"
      cat /tmp/check-assets.diff
      rm -f /tmp/check-assets.diff
      exit 1
    fi
    rm -f /tmp/check-assets.diff
    ;;

  write)
    cp "$TMP_MANIFEST" "$MANIFEST_PATH"
    echo "check-assets: wrote manifest to $MANIFEST_PATH"
    ;;

  *)
    echo "check-assets: unknown mode '$MODE' (expected 'check' or 'write')" >&2
    exit 1
    ;;
esac
