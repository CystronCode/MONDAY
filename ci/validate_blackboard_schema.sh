#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[validate_blackboard] Running generator into $TMP_DIR ..."
python3 "$ROOT/tools/generate_blackboard.py" \
    "$ROOT/blackboard_schema.toml" \
    "$TMP_DIR"

echo "[validate_blackboard] Diffing generated files against committed ..."
diff "$TMP_DIR/blackboard_accessors.h" "$ROOT/src/generated/blackboard_accessors.h"
diff "$TMP_DIR/blackboard_accessors.c" "$ROOT/src/generated/blackboard_accessors.c"

echo "[validate_blackboard] CLEAN — committed files match generator output."

