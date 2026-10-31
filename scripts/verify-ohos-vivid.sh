#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
TASK_TMP=$(mktemp -d)
trap 'rm -f "$TASK_TMP/state" "$TASK_TMP/state.exe" "$TASK_TMP/reader" "$TASK_TMP/reader.exe" "$TASK_TMP/output" "$TASK_TMP/output.exe" "$TASK_TMP/vivid-output.inc"; rmdir "$TASK_TMP"' EXIT

# These checks tie the host tests to the production Vulkan integration. They
# are source-contract checks, not evidence of correct device presentation.
grep -Fq "files('video/out/ohos_vivid.cpp')" "$MPV_SOURCE/meson.build"
grep -Fq 'ohos_vivid_read_ex(buffer, &v->vivid' "$MPV_SOURCE/video/out/hwdec/hwdec_ohcodec_pl.c"
grep -Fq 'vo_ohos_vivid_epoch(vo)' "$MPV_SOURCE/video/out/hwdec/hwdec_ohcodec_pl.c"
grep -Fq 'ohos_vivid_matches(frame, token, ctx->vivid_epoch)' "$MPV_SOURCE/video/out/ohos_common.c"
grep -Fq 'OH_HDR_DYNAMIC_METADATA' "$MPV_SOURCE/video/out/ohos_common.c"
grep -Fq '[Vivid] staged-before-acquire' "$MPV_SOURCE/video/out/ohos_common.c"
grep -Fq '[Vivid] last-flushed' "$MPV_SOURCE/video/out/ohos_common.c"

"${PYTHON_FOR_BUILD:-python3}" - "$MPV_SOURCE" "$TASK_TMP" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
for relative in ('options/options.c', 'options/options.h',
                 'video/out/ohos_common.c', 'video/out/vo_gpu_next.c',
                 'video/out/hwdec/hwdec_ohcodec_pl.c'):
    code = (root / relative).read_text()
    assert 'ohos-hdr-vivid' not in code and 'ohos_hdr_vivid' not in code, \
        'HDR Vivid transport must be automatic, without a configuration switch'
source = (root / 'video/out/vo_gpu_next.c').read_text()
draw_start = source.index('static bool draw_frame(')
draw = source[draw_start:source.index('static void flip_page(', draw_start)]
assert draw.index('vo_ohos_vivid_prepare(') < draw.index('pl_swapchain_start_frame('), \
    'Vivid metadata must be staged before acquiring the output swapchain frame'
reset = source[source.index('case VOCTRL_RESET:') + len('case VOCTRL_RESET:'):]
assert 'vo_ohos_vivid_reset(vo)' in reset.split('case ', 1)[0], \
    'VOCTRL_RESET must invalidate the Vivid epoch'
assert 'vivid_ten_bit_output' in draw and 'ohosvk' in draw, \
    'Passthrough requires actual high-bit-depth Vulkan output'
assert draw.index('if (vivid_result < 0)') < draw.index('pl_swapchain_start_frame('), \
    'Unsafe basic-output restoration must withhold the frame before acquire'
flip = source[source.index('static void flip_page('):]
assert 'p->vivid_frame_pending = valid && vivid_passthrough' in draw, \
    'A failed render must not qualify for Vivid output audit'
assert 'submitted && p->vivid_frame_pending' in flip, \
    'Output audit requires a successfully rendered and submitted Vivid frame'

# Compile these exact production functions in the host platform harness. The
# renderer's unrelated implementation is neither copied nor stub-compiled.
common = (root / 'video/out/ohos_common.c').read_text()
first = common.index('static bool vivid_restore_basic(')
last = common.index('void vo_ohos_vivid_audit(', first)
(Path(sys.argv[2]) / 'vivid-output.inc').write_text(common[first:last])
PY

"${CC_FOR_BUILD:-${CC:-cc}}" -std=c11 -Wall -Wextra -Werror \
    -I"$MPV_SOURCE" "$ROOT/tests/ohos-vivid.c" -o "$TASK_TMP/state"
"$TASK_TMP/state"
"${CXX_FOR_BUILD:-${CXX:-c++}}" -std=c++17 -Wall -Wextra -Werror \
    -I"$ROOT/tests/vivid-shim" -I"$MPV_SOURCE" \
    "$ROOT/tests/ohos-vivid-reader.cpp" "$MPV_SOURCE/video/out/ohos_vivid.cpp" \
    -o "$TASK_TMP/reader"
"$TASK_TMP/reader"
"${CC_FOR_BUILD:-${CC:-cc}}" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT/tests/vivid-shim" -I"$MPV_SOURCE" -I"$TASK_TMP" \
    "$ROOT/tests/ohos-vivid-output.c" -o "$TASK_TMP/output"
"$TASK_TMP/output"
echo 'HDR Vivid frame, native-reader and output host checks passed (not device passthrough validation)'
