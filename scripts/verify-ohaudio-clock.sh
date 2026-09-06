#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
CHECK_DIR="$ROOT_DIR/libmpv/ohaudio-clock-test"
mkdir -p "$CHECK_DIR"
cc -std=c11 -O2 -pthread -I"$ROOT_DIR/libmpv/mpv" \
  "$ROOT_DIR/tests/ohaudio-clock.c" -o "$CHECK_DIR/clock-test"
"$CHECK_DIR/clock-test"
# Reject accidental reintroduction of per-callback timing statistics.
if grep -Eq 'OHAudio 10s|report_stats|callback_total_ns|read_total_ns|query_total_ns' \
    "$ROOT_DIR/libmpv/mpv/audio/out/ao_ohaudio.c"; then
  echo 'OHAudio performance counters must remain removed' >&2
  exit 1
fi
