#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
MPV_SOURCE=${MPV_SOURCE:-$ROOT/libmpv/mpv}
FFMPEG_SOURCE=${FFMPEG_SOURCE:-$ROOT/libmpv/ffmpeg}
grep -Fq 'char *bluray_network_root;' "$MPV_SOURCE/options/options.h"
grep -Fq '"network-root", OPT_STRING(bluray_network_root)' "$MPV_SOURCE/stream/stream_bluray.c"
grep -Fq 'bd_network_open(s, device_tmp, network_root) : bd_open(device_tmp, NULL)' "$MPV_SOURCE/stream/stream_bluray.c"
source="$MPV_SOURCE/stream/bluray_network.h"
grep -Fq 'bd_open_files(bd, ctx, bd_network_open_dir, bd_network_open_file)' "$source"
grep -Fq 'mp_cancel_test(ctx->parent->cancel)' "$source"
grep -Fq '"max_redirects", "0"' "$source"
grep -Fq '"protocol_whitelist", "http,https,tcp,tls"' "$source"
grep -Fq 'AVIO_SEEKABLE_NORMAL' "$source"
grep -Fq '"max_redirects"' "$FFMPEG_SOURCE/libavformat/http.c"
echo 'Network Blu-ray navigation, cancellation and redirect policy checks passed'
