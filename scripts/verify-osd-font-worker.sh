#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
CHECK_DIR="$ROOT_DIR/libmpv/osd-font-worker-test"
mkdir -p "$CHECK_DIR"
python3 - "$ROOT_DIR/libmpv/mpv/sub/osd_libass.c" "$CHECK_DIR/osd-font-worker.inc" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
begin = source.index('static MP_THREAD_VOID preload_font_thread(')
end = source.index('static void destroy_ass_renderer', begin)
pathlib.Path(sys.argv[2]).write_text(source[begin:end])
PY
cc -std=c11 -O2 -pthread -Wall -Wextra -Wno-unused-variable -I"$CHECK_DIR" \
  "$ROOT_DIR/tests/osd-font-worker.c" -o "$CHECK_DIR/font-worker-test"
timeout 15s "$CHECK_DIR/font-worker-test"
