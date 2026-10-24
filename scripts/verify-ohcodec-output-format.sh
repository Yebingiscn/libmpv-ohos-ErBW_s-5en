#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
FFMPEG_SOURCE=${FFMPEG_SOURCE:-"$ROOT_DIR/libmpv/ffmpeg"}
CHECK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ohcodec-output-format.XXXXXX")
cleanup() {
    rm -f -- "$CHECK_DIR/ohcodec-output-format.inc" \
        "$CHECK_DIR/output-format-test" "$CHECK_DIR/output-format-test.exe"
    rmdir -- "$CHECK_DIR"
}
trap cleanup EXIT

"${PYTHON:-python3}" - "$FFMPEG_SOURCE/libavcodec/ohdec.c" "$CHECK_DIR/ohcodec-output-format.inc" <<'PY'
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
functions = []
for result, name in (
    ("int", "oh_decode_init_hwframes"),
    ("enum AVPixelFormat", "oh_decode_get_surface_format"),
    ("void", "oh_decode_on_stream_changed"),
):
    pattern = r"^static " + result + r" " + name + r"\([\s\S]+?^\}"
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if not match:
        raise SystemExit("Missing patched function: " + name)
    functions.append(match.group(0))
pathlib.Path(sys.argv[2]).write_text("\n\n".join(functions) + "\n", encoding="utf-8")
PY

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -I"$CHECK_DIR" \
    "$ROOT_DIR/tests/ohcodec-output-format.c" -o "$CHECK_DIR/output-format-test"
"$CHECK_DIR/output-format-test"
