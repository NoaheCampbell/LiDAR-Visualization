#!/bin/bash

# Check if --no-noise is passed
NOISE_FLAG=""
if [[ "$1" == "--no-noise" ]]; then
    NOISE_FLAG="--no-noise"
fi

# Start rover emulator instances for IDs 1-5
PIDS=()

for ID in {1..5}; do
    ./rover_emulator "$ID" $NOISE_FLAG &  # Append the noise flag if specified
    PIDS+=($!)  # Store PID
done

# Function to kill all child processes on script exit
cleanup() {
    echo "Terminating all rover instances..."
    for PID in "${PIDS[@]}"; do
        kill "$PID" 2>/dev/null
    done
    exit 0
}

# Trap signals to ensure cleanup happens
trap cleanup SIGINT SIGTERM EXIT

# Wait for all processes to finish
wait
