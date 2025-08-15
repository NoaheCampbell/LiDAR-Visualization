#!/bin/bash

set -euo pipefail

# Always resolve paths relative to the repo root (script directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/build/rover_emulator"

# Check if --no-noise is passed
NOISE_FLAG=""
if [[ "${1:-}" == "--no-noise" ]]; then
    NOISE_FLAG="--no-noise"
fi

if [[ ! -x "$BINARY" ]]; then
  echo "Error: $BINARY not found or not executable. Build the project first." >&2
  exit 1
fi

# Start rover emulator instances for IDs 1-5 from repo root so data paths resolve
PIDS=()
for ID in {1..5}; do
    ( cd "$SCRIPT_DIR" && "$BINARY" "$ID" $NOISE_FLAG ) &
    PIDS+=($!)
done

cleanup() {
    echo "Terminating all rover instances..."
    for PID in "${PIDS[@]}"; do
        kill "$PID" 2>/dev/null || true
    done
    exit 0
}
trap cleanup SIGINT SIGTERM EXIT
wait
