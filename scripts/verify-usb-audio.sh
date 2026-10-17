#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
TMP_DIR=$(mktemp -d)
trap 'rm -f "$TMP_DIR/usb-test" "$TMP_DIR/dsd-test" "$TMP_DIR/pcm-test" "$TMP_DIR/lifecycle-test" "$TMP_DIR/usb-functions.inc"; rmdir "$TMP_DIR"' EXIT
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/usb-audio-protocol.c" -o "$TMP_DIR/usb-test"
"$TMP_DIR/usb-test"
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/dsd-output.c" -o "$TMP_DIR/dsd-test"
"$TMP_DIR/dsd-test"
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$MPV_SOURCE" \
  "$ROOT/tests/usb-pcm-packing.c" -o "$TMP_DIR/pcm-test"
"$TMP_DIR/pcm-test"
awk '/^static struct slot \*completed_slot\(/ || /^static bool reap_cancelled\(/ || /^static bool stop_thread\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' \
  "$MPV_SOURCE/audio/out/ao_usb_exclusive.c" > "$TMP_DIR/usb-functions.inc"
awk '/^void ao_set_gain\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' \
  "$MPV_SOURCE/audio/out/ao.c" >> "$TMP_DIR/usb-functions.inc"
"${CC_FOR_BUILD:-cc}" -std=c11 -Wall -Wextra -Werror -I"$TMP_DIR" \
  "$ROOT/tests/usb-lifecycle.c" -o "$TMP_DIR/lifecycle-test"
"$TMP_DIR/lifecycle-test"
