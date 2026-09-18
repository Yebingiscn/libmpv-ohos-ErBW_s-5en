#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
FFMPEG_SOURCE=${FFMPEG_SOURCE:-$ROOT/libmpv/ffmpeg}
TMP_DIR=$(mktemp -d)
trap 'rm -f "$TMP_DIR/vivid-test"; rmdir "$TMP_DIR"' EXIT
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror \
  -I"$MPV_SOURCE" -I"$FFMPEG_SOURCE" "$ROOT/tests/ohaudio-vivid-packet.c" \
  -o "$TMP_DIR/vivid-test"
"$TMP_DIR/vivid-test"
echo "Audio Vivid metadata/frame contract checks passed"
