#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
CHECK_DIR="$ROOT_DIR/libmpv/dts-pts-repair-test"
mkdir -p "$CHECK_DIR"
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT_DIR/libmpv/mpv" \
  "$ROOT_DIR/tests/dts-pts-repair.c" -lm -o "$CHECK_DIR/repair-test"
"$CHECK_DIR/repair-test"
