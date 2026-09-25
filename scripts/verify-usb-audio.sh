#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
TMP_DIR=$(mktemp -d)
trap 'rm -f "$TMP_DIR/usb-test" "$TMP_DIR/dsd-test"; rmdir "$TMP_DIR"' EXIT
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/usb-audio-protocol.c" -o "$TMP_DIR/usb-test"
"$TMP_DIR/usb-test"
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/dsd-output.c" -o "$TMP_DIR/dsd-test"
"$TMP_DIR/dsd-test"
