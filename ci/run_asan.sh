#!/usr/bin/env bash
set -euo pipefail

# ci/run_asan.sh
# Configures, builds, and runs the integration test suite under AddressSanitizer.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
cd "$ROOT"

echo "[run_asan] Configuring CMake with AddressSanitizer..."
cmake --preset asan

echo "[run_asan] Building target monday-core (limited to 2 parallel jobs to save CPU)..."
cmake --build build-asan --target monday-core -j 2

echo "[run_asan] Running integration test suite under AddressSanitizer (60 seconds)..."
./build-asan/monday-core --integration-test-suite --duration 60

echo "[run_asan] CLEAN — integration test suite completed under ASan with 0 errors."
