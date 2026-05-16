#!/usr/bin/env bash

set -euo pipefail

DOSBOX_PATH="${DOSBOX_PATH:-}"
CONFIG_PATH="${1:-assets/DOSBox/wolf3d.conf}"
ASSETS_BASE="${2:-assets/base}"
CAPTURE_DIR="${3:-build/original-capture}"
MANIFEST_PATH="${4:-scripts/wolf3d-autoshot-original.sha256}"
TIMEOUT_SECONDS="${5:-120}"
EXPECTED_SHOTS="${6:-0}"
AUTOEXEC_COMMANDS="${7:-}"
VERBOSE="${VERBOSE:-0}"
PATTERN="autoshot_*.bmp"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -n "${AUTOEXEC_COMMANDS}" ]; then
  # commands should be a single string with each command separated by semicolons.
  IFS=';' read -r -a AUTOEXECS <<< "$AUTOEXEC_COMMANDS"
fi

CONFIG_PATH="$REPO_ROOT/${CONFIG_PATH#./}"
ASSETS_BASE="$REPO_ROOT/${ASSETS_BASE#./}"
CAPTURE_DIR="$REPO_ROOT/${CAPTURE_DIR#./}"
MANIFEST_PATH="$REPO_ROOT/${MANIFEST_PATH#./}"

if [ ! -d "$ASSETS_BASE" ]; then
  echo "capture-original: assets base not found: $ASSETS_BASE" >&2
  exit 1
fi
if [ ! -f "$CONFIG_PATH" ]; then
  echo "capture-original: config not found: $CONFIG_PATH" >&2
  exit 1
fi

if [ -z "$DOSBOX_PATH" ]; then
  if [ -f "$REPO_ROOT/assets/DOSBox/DOSBox.exe" ]; then
    DOSBOX_PATH="$REPO_ROOT/assets/DOSBox/DOSBox.exe"
  else
    DOSBOX_PATH="$(command -v dosbox || true)"
  fi
fi
if [ -z "$DOSBOX_PATH" ]; then
  echo "capture-original: DOSBox executable not found: ${DOSBOX_PATH:-(none)}" >&2
  exit 1
fi
if [ -d "$DOSBOX_PATH" ]; then
  echo "capture-original: DOSBox path is a directory: $DOSBOX_PATH" >&2
  exit 1
fi
if [ ! -f "$DOSBOX_PATH" ] && [ ! -x "$DOSBOX_PATH" ]; then
  echo "capture-original: DOSBox path is not executable: $DOSBOX_PATH" >&2
  exit 1
fi

mkdir -p "$CAPTURE_DIR"

if [ "${#AUTOEXECS[@]:-0}" -eq 0 ]; then
  AUTOEXECS=(
    "mount C \"${ASSETS_BASE}\""
    "C:"
    "launcher\\start.bat"
  )
fi

# Ensure captures only include this run.
if [ -d "$CAPTURE_DIR" ]; then
  rm -f "$CAPTURE_DIR"/autoshot_*.bmp 2>/dev/null || true
fi

BASE_CONFIG_NO_AUTOEXEC="$(awk '
  BEGIN {skip=0}
  /^[[:space:]]*\[autoexec\][[:space:]]*$/ {
    skip=1
    next
  }
  /^\[/ {skip=0}
  skip==0 {print}
' "$CONFIG_PATH")"

TMP_CONFIG="$(mktemp)"
{
  printf "%s\n\n" "$BASE_CONFIG_NO_AUTOEXEC"
  printf "[dosbox]\ncaptures=%s\n\n" "$CAPTURE_DIR"
  printf "[autoexec]\n@ECHO OFF\n"
  for cmd in "${AUTOEXECS[@]}"; do
    printf "%s\n" "$cmd"
  done
  printf "exit\n"
} > "$TMP_CONFIG"

if [ "$VERBOSE" = "1" ]; then
  echo "capture-original: using config: $TMP_CONFIG"
  echo "capture-original: capture dir: $CAPTURE_DIR"
  echo "capture-original: timeout: ${TIMEOUT_SECONDS}s"
fi

"$DOSBOX_PATH" -conf "$TMP_CONFIG" &
DOSBOX_PID=$!

cleanup() {
  if kill -0 "$DOSBOX_PID" 2>/dev/null; then
    kill "$DOSBOX_PID" 2>/dev/null || true
  fi
  wait "$DOSBOX_PID" 2>/dev/null || true
  rm -f "$TMP_CONFIG"
}
trap cleanup EXIT

watcher_pid=
if [ "$TIMEOUT_SECONDS" -gt 0 ]; then
  (
    sleep "$TIMEOUT_SECONDS"
    kill "$DOSBOX_PID" 2>/dev/null || true
  ) &
  watcher_pid=$!
fi

wait "$DOSBOX_PID" || true
if [ -n "$watcher_pid" ]; then
  kill "$watcher_pid" 2>/dev/null || true
fi

shopt -s nullglob
CANDIDATE_SHOTS=( "$CAPTURE_DIR"/$PATTERN )
shopt -u nullglob

readarray -t SHOTS < <(printf '%s\n' "${CANDIDATE_SHOTS[@]}" | sort)
if [ "${#SHOTS[@]}" -eq 0 ]; then
  echo "capture-original: no autoshot_*.bmp files found in $CAPTURE_DIR" >&2
  exit 1
fi
if [ "$EXPECTED_SHOTS" -gt 0 ] && [ "${#SHOTS[@]}" -ne "$EXPECTED_SHOTS" ]; then
  echo "capture-original: screenshot count mismatch. expected=$EXPECTED_SHOTS got=${#SHOTS[@]}" >&2
  exit 1
fi

rm -f "$MANIFEST_PATH"
for shot in "${SHOTS[@]}"; do
  shot_name="$(basename "$shot")"
  hash="$(sha256sum "$shot" | awk '{print $1}')"
  printf '%s  %s\n' "$hash" "$shot_name" >> "$MANIFEST_PATH"
done

echo "capture-original: wrote manifest: $MANIFEST_PATH"
echo "capture-original: captured ${#SHOTS[@]} screenshots in $CAPTURE_DIR"
