#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
LIBPLACEBO_SOURCE=${LIBPLACEBO_SOURCE:-$ROOT/libmpv/libplacebo}
for pair in 'src/opengl/gpu.c:pl_maleoon_name' \
            'src/vulkan/gpu.c:pl_maleoon_name' \
            'src/shaders/custom_mpv.c:@sweetvideo-maleoon-artcnn-8x8' \
            'src/shaders/custom_mpv.c:pl_maleoon_workgroup'; do
    grep -Fq "${pair#*:}" "$LIBPLACEBO_SOURCE/${pair%%:*}" || {
        echo "Missing Maleoon GPU invariant: $pair" >&2; exit 1;
    }
done
TASK_TMP=$(mktemp -d)
trap 'rm -f "$TASK_TMP/policy" "$TASK_TMP/policy.exe"; rmdir "$TASK_TMP"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$LIBPLACEBO_SOURCE/src" \
    "$ROOT/tests/maleoon-policy.c" -o "$TASK_TMP/policy"
"$TASK_TMP/policy"
echo 'Maleoon source and host policy checks passed (not a GPU benchmark)'
