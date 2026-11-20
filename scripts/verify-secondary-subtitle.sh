#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
grep -Fq '"secondary-sub-scale", OPT_FLOAT(sub_scale[1])' "$MPV_SOURCE/options/options.c"
grep -Fq '.sub_scale[1] = 1' "$MPV_SOURCE/options/options.c"
grep -Fq 'float sub_scale[2];' "$MPV_SOURCE/options/options.h"
for source in sub/sd_ass.c sub/sd_lavc.c; do
    grep -Fq 'shared_opts->sub_scale[sd->order]' "$MPV_SOURCE/$source"
done
echo 'Independent secondary subtitle scale source checks passed'
