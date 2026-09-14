#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
CHECK_DIR="$ROOT_DIR/libmpv/ohcodec-decode-hints-test"
mkdir -p "$CHECK_DIR"
python3 - "$ROOT_DIR/libmpv/ffmpeg/libavcodec/ohdec.c" "$CHECK_DIR/ohcodec-decode-hints.inc" <<'PY'
import pathlib, re, sys
source = pathlib.Path(sys.argv[1]).read_text()
functions = []
for name in ('oh_decode_update_playback_policy', 'avcodec_ohcodec_set_playback_speed',
             'avcodec_ohcodec_set_decode_hints'):
    match = re.search(r'^(?:static void|int) ' + name + r'\([\s\S]+?\n\}\n', source, re.M)
    if not match:
        raise SystemExit('Missing patched function: ' + name)
    functions.append(match.group(0))
pathlib.Path(sys.argv[2]).write_text('\n'.join(functions))
PY
cc -std=c11 -O2 -Wall -Wextra -Werror -I"$CHECK_DIR" \
  "$ROOT_DIR/tests/ohcodec-decode-hints.c" -lm -o "$CHECK_DIR/hints-test"
"$CHECK_DIR/hints-test"
