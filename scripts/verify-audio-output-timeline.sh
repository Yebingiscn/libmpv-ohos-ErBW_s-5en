#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
CHECK_DIR="$ROOT_DIR/libmpv/audio-output-timeline-test"
mkdir -p "$CHECK_DIR"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT_DIR/libmpv/mpv" \
  "$ROOT_DIR/tests/audio-output-timeline.c" -lm -o "$CHECK_DIR/timeline-test"
"$CHECK_DIR/timeline-test"
