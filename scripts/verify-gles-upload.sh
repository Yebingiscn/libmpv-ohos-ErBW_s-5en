#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
LIBPLACEBO_SOURCE=${LIBPLACEBO_SOURCE:-$ROOT/libmpv/libplacebo}
grep -Fq '#include "upload_pool.h"' "$LIBPLACEBO_SOURCE/src/opengl/gpu.h"
grep -Fq 'gles_upload_destroy(&p->upload_pool' "$LIBPLACEBO_SOURCE/src/opengl/gpu.c"
grep -Fq 'gles_upload_prepare(&p->upload_pool' "$LIBPLACEBO_SOURCE/src/opengl/gpu_tex.c"
grep -Fq 'gles_upload_submit(&p->upload_pool' "$LIBPLACEBO_SOURCE/src/opengl/gpu_tex.c"
grep -Fq 'p->gles_ver >= 30' "$LIBPLACEBO_SOURCE/src/opengl/gpu_tex.c"
grep -Fq 'if (params->callback && ok && staging >= 0)' "$LIBPLACEBO_SOURCE/src/opengl/gpu_tex.c"
TASK_TMP=$(mktemp -d)
trap 'rm -f "$TASK_TMP/pool" "$TASK_TMP/pool.exe"; rmdir "$TASK_TMP"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$LIBPLACEBO_SOURCE/src" \
    "$ROOT/tests/gles-upload-pool.c" -o "$TASK_TMP/pool"
"$TASK_TMP/pool"
echo 'GLES upload source and host tests passed (not a device performance test)'
