#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
TMP_DIR=$(mktemp -d)
trap 'rm -f "$TMP_DIR/eac3-test"; rmdir "$TMP_DIR"' EXIT
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/ohaudio-eac3-packet.c" -o "$TMP_DIR/eac3-test"
"$TMP_DIR/eac3-test"
