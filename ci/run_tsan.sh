#!/usr/bin/env bash
set -euo pipefail

# ci/run_tsan.sh
# Configures, builds, and runs the integration test suite under ThreadSanitizer.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
cd "$ROOT"

echo "[run_tsan] Configuring CMake with ThreadSanitizer..."
cmake --preset tsan

echo "[run_tsan] Building target monday-core (limited to 2 parallel jobs to save CPU)..."
cmake --build build-tsan --target monday-core -j 2

echo "[run_tsan] Running integration test suite under ThreadSanitizer (60 seconds)..."
# We run under setarch $(uname -m) -R to disable ASLR, which avoids the TSan unexpected memory mapping error on WSL2.
setarch $(uname -m) -R ./"build-tsan"/monday-core --integration-test-suite --duration 60

echo "[run_tsan] CLEAN — integration test suite completed under TSan with 0 races."
