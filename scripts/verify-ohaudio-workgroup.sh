#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
TMP_DIR=$(mktemp -d)
trap 'rm -f "$TMP_DIR/workgroup-test" "$TMP_DIR/workgroup-test.exe" "$TMP_DIR/callback-test" "$TMP_DIR/callback-test.exe" "$TMP_DIR/workgroup-callback.inc"; rmdir "$TMP_DIR"' EXIT
"${CC_FOR_BUILD:-cc}" -std=gnu11 -O2 -pthread -Wall -Wextra -Werror \
    -I"$ROOT/tests/workgroup-shim" -I"$MPV_SOURCE" \
    "$ROOT/tests/ohaudio-workgroup.c" -o "$TMP_DIR/workgroup-test"
"$TMP_DIR/workgroup-test"
"${PYTHON_FOR_BUILD:-python3}" "$ROOT/tests/check-ohaudio-workgroup.py" "$MPV_SOURCE"
"${PYTHON_FOR_BUILD:-python3}" "$ROOT/tests/check-ohaudio-workgroup.py" "$MPV_SOURCE" \
    --emit-callback > "$TMP_DIR/workgroup-callback.inc"
"${CC_FOR_BUILD:-cc}" -std=gnu11 -O2 -pthread -Wall -Wextra -Werror \
    -I"$ROOT/tests/workgroup-shim" -I"$MPV_SOURCE" -I"$TMP_DIR" \
    "$ROOT/tests/ohaudio-workgroup-callback.c" -o "$TMP_DIR/callback-test"
"$TMP_DIR/callback-test"
