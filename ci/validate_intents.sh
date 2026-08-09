#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
SCHEMA="$ROOT/data/intents.schema.json"
INTENTS="$ROOT/data/intents.jsonl"
FAIL=0

echo "[validate_intents] Validating $INTENTS ..."

while IFS= read -r line || [[ -n "$line" ]]; do
    tmpfile=$(mktemp /tmp/intent_XXXXXX.json)
    echo "$line" > "$tmpfile"
    if ! ajv validate -s "$SCHEMA" -d "$tmpfile" --spec=draft7 2>/dev/null; then
        echo "FAIL: $line"
        FAIL=1
    fi
    rm -f "$tmpfile"
done < "$INTENTS"

if [ $FAIL -ne 0 ]; then
    echo "[validate_intents] FAILED — one or more intents did not validate."
    exit 1
fi
echo "[validate_intents] All intents valid."
