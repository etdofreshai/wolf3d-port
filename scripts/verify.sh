#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${1:-build}"
TIMEOUT_SECONDS="${WOLF3D_VERIFY_TIMEOUT:-12}"
VERBOSE="${WOLF3D_VERIFY_VERBOSE:-0}"
EXE_NAME="wolf3d"

SHOT_DIR=""

# Candidate executable locations (single-config and multi-config layouts).
EXE_CANDIDATES="
${BUILD_DIR}/${EXE_NAME}
${BUILD_DIR}/Debug/${EXE_NAME}
${BUILD_DIR}/Debug/${EXE_NAME}.exe
${BUILD_DIR}/Release/${EXE_NAME}
${BUILD_DIR}/Release/${EXE_NAME}.exe
"

EXE=""
for candidate in $EXE_CANDIDATES; do
  if [ -f "$candidate" ]; then
    EXE="$candidate"
    break
  fi
done

if [ -z "$EXE" ]; then
  # Last-resort: first matching executable under build root.
  EXE="$(find "$BUILD_DIR" -maxdepth 3 -type f \( -name "$EXE_NAME" -o -name "${EXE_NAME}.exe" \) | head -n 1 || true)"
fi

if [ -z "$EXE" ]; then
  echo "Could not find executable in $BUILD_DIR"
  exit 1
fi

SHOT_DIR="$(dirname "$EXE")"
ABS_SHOT_DIR="$(cd "$SHOT_DIR" && pwd)"
LOG_FILE="$ABS_SHOT_DIR/wolf3d-verify.log"
HASH_FILE="$ABS_SHOT_DIR/wolf3d-autoshot.sha256"
STDOUT_FILE="$ABS_SHOT_DIR/wolf3d-verify.stdout.log"
STDERR_FILE="$ABS_SHOT_DIR/wolf3d-verify.stderr.log"
PORT_MANIFEST_NORM=""
REF_MANIFEST_NORM=""
trap 'rm -f "$PORT_MANIFEST_NORM" "$REF_MANIFEST_NORM"' EXIT
: > "$LOG_FILE"
: > "$HASH_FILE"
: > "$STDOUT_FILE"
: > "$STDERR_FILE"
printf 'Using executable: %s\n' "$EXE"

export WOLF3D_SCREENSHOT="${WOLF3D_SCREENSHOT:-1}"
export WOLF3D_SCREENSHOT_STRIDE="${WOLF3D_SCREENSHOT_STRIDE:-18}"
export WOLF3D_SCREENSHOT_COUNT="${WOLF3D_SCREENSHOT_COUNT:-9}"
export WOLF3D_SCREENSHOT_SKIP="${WOLF3D_SCREENSHOT_SKIP:-0}"
export WOLF3D_SCREENSHOT_EXIT="${WOLF3D_SCREENSHOT_EXIT:-1}"
export WOLF3D_VERIFY_DEMO="${WOLF3D_VERIFY_DEMO:-0}"
export WOLF3D_REFERENCE_MANIFEST="${WOLF3D_REFERENCE_MANIFEST:-}"
EXPECTED_SHOTS="${WOLF3D_SCREENSHOT_EXPECTED:-$WOLF3D_SCREENSHOT_COUNT}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKDIR="$(pwd)"

if [ "$VERBOSE" = "1" ]; then
  echo "Running with timeout ${TIMEOUT_SECONDS}s"
  echo "WOLF3D_SCREENSHOT=$WOLF3D_SCREENSHOT"
  echo "WOLF3D_SCREENSHOT_STRIDE=$WOLF3D_SCREENSHOT_STRIDE"
  echo "WOLF3D_SCREENSHOT_COUNT=$WOLF3D_SCREENSHOT_COUNT"
  echo "WOLF3D_SCREENSHOT_SKIP=$WOLF3D_SCREENSHOT_SKIP"
  echo "WOLF3D_SCREENSHOT_EXIT=$WOLF3D_SCREENSHOT_EXIT"
  echo "WOLF3D_VERIFY_DEMO=$WOLF3D_VERIFY_DEMO"
  echo "WOLF3D_REFERENCE_MANIFEST=$WOLF3D_REFERENCE_MANIFEST"
  if [ -n "$EXPECTED_SHOTS" ]; then
    echo "WOLF3D_SCREENSHOT_EXPECTED=$EXPECTED_SHOTS"
  fi
  echo "Shooting directory: $SHOT_DIR"
  echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"
fi

rm -f "$SHOT_DIR"/autoshot_*.bmp

run_binary() {
  local exe_path="$1"
  local log_path="$2"
  local exe_dir
  local exe_base

  exe_dir=$(dirname "$exe_path")
  exe_base=$(basename "$exe_path")

  if command -v timeout >/dev/null 2>&1; then
    (cd "$exe_dir" && timeout "${TIMEOUT_SECONDS}s" "./$exe_base" > "$STDOUT_FILE" 2> "$STDERR_FILE")
  else
    (cd "$exe_dir" && "./$exe_base") > "$log_path" 2>&1 &
    local pid=$!

    (sleep "${TIMEOUT_SECONDS}s"; kill -TERM "$pid" 2>/dev/null) &
    local killer_pid=$!

    wait "$pid"
    local run_status=$?
    kill "$killer_pid" 2>/dev/null || true
    wait "$killer_pid" 2>/dev/null || true
    return "$run_status"
  fi
}

set +e
run_binary "$EXE" "$LOG_FILE"
BIN_STATUS=$?
set -e

if [[ ! -f "$LOG_FILE" ]]; then
  echo "verify: expected log file not found: $LOG_FILE"
  exit 1
fi
if [[ $BIN_STATUS -ne 0 ]] && [[ $BIN_STATUS -ne 124 ]] && [[ $BIN_STATUS -ne 143 ]]; then
  # 124 is timeout(1) exit when it kills the process after the wall-clock budget.
  # Any other exit status indicates executable startup/runtime failure.
  echo "verify: binary exited with failure code $BIN_STATUS"
  exit 1
fi

if [ -d "$SHOT_DIR" ]; then
  shopt -s nullglob
  SHOT_COUNT=0
  while IFS= read -r file; do
    if [ -z "$file" ]; then
      continue
    fi
    SHOT_COUNT=$((SHOT_COUNT + 1))
    file_base="$(basename "$file")"
    if command -v sha256sum >/dev/null 2>&1; then
      printf '%s  %s\n' "$(sha256sum "$file" | cut -d' ' -f1)" "$file_base" | tee -a "$HASH_FILE"
    else
      printf '%s  %s\n' "$(shasum -a 256 "$file" | cut -d' ' -f1)" "$file_base" | tee -a "$HASH_FILE"
    fi
  done < <(printf '%s\n' "$SHOT_DIR"/autoshot_*.bmp | sort)
  if (( SHOT_COUNT == 0 )); then
    echo "verify: no autoshot_*.bmp files found in $SHOT_DIR"
    exit 1
  fi
else
  SHOT_COUNT=0
fi

if [ -n "$EXPECTED_SHOTS" ]; then
  if [ "$SHOT_COUNT" -ne "$EXPECTED_SHOTS" ]; then
    echo "verify: expected ${EXPECTED_SHOTS} screenshot(s), got ${SHOT_COUNT}"
    exit 1
  fi
fi

echo "verify: captured $SHOT_COUNT screenshot(s)"

if [ -n "$WOLF3D_REFERENCE_MANIFEST" ]; then
  if [[ "$WOLF3D_REFERENCE_MANIFEST" = /* ]]; then
    REF_MANIFEST_PATH="$WOLF3D_REFERENCE_MANIFEST"
  else
    if [ -f "$SCRIPT_DIR/$WOLF3D_REFERENCE_MANIFEST" ]; then
      REF_MANIFEST_PATH="$SCRIPT_DIR/$WOLF3D_REFERENCE_MANIFEST"
    elif [ -f "$WORKDIR/$WOLF3D_REFERENCE_MANIFEST" ]; then
      REF_MANIFEST_PATH="$WORKDIR/$WOLF3D_REFERENCE_MANIFEST"
    else
      REF_MANIFEST_PATH=""
    fi
  fi

  if [ ! -f "$REF_MANIFEST_PATH" ]; then
    echo "verify: reference manifest not found: $REF_MANIFEST_PATH"
    exit 1
  fi

  REF_MANIFEST_NORM="$(mktemp)"
  PORT_MANIFEST_NORM="$(mktemp)"
  tr -d '\r' < "$REF_MANIFEST_PATH" | sed -e $'1s/\xEF\xBB\xBF//' > "$REF_MANIFEST_NORM"
  tr -d '\r' < "$HASH_FILE" | sed -e $'1s/\xEF\xBB\xBF//' > "$PORT_MANIFEST_NORM"
  if ! diff -u "$REF_MANIFEST_NORM" "$PORT_MANIFEST_NORM" >/dev/null; then
    echo "verify: reference manifest mismatch"
    diff -u "$REF_MANIFEST_NORM" "$PORT_MANIFEST_NORM"
    exit 1
  fi

  echo "verify: reference manifest matched: $REF_MANIFEST_PATH"
fi

if [ -f "$STDOUT_FILE" ]; then
  echo "=== stdout ===" >> "$LOG_FILE"
  cat "$STDOUT_FILE" >> "$LOG_FILE"
fi
if [ -f "$STDERR_FILE" ]; then
  if [ -s "$STDERR_FILE" ]; then
    echo "=== stderr ===" >> "$LOG_FILE"
    cat "$STDERR_FILE" >> "$LOG_FILE"
  fi
fi

if [ -f "$LOG_FILE" ]; then
  tail -n 20 "$LOG_FILE"
fi
