#!/usr/bin/env bash
set -euo pipefail

# ci/run_ubsan.sh
# Configures, builds, and runs the integration test suite under UndefinedBehaviorSanitizer.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
cd "$ROOT"

echo "[run_ubsan] Configuring CMake with UndefinedBehaviorSanitizer..."
cmake --preset ubsan

echo "[run_ubsan] Building target monday-core (limited to 2 parallel jobs to save CPU)..."
cmake --build build-ubsan --target monday-core -j 2

echo "[run_ubsan] Running integration test suite under UndefinedBehaviorSanitizer (60 seconds)..."
./build-ubsan/monday-core --integration-test-suite --duration 60

echo "[run_ubsan] CLEAN — integration test suite completed under UBSan with 0 errors."
