#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
FFMPEG_SOURCE=${FFMPEG_SOURCE:-$ROOT/libmpv/ffmpeg}
SRC="$FFMPEG_SOURCE/libavformat/ohos_http3.c"
for marker in 'dlopen(names[i], RTLD_NOW | RTLD_LOCAL)' \
  'HMS_Rcp_QuicConnStreamWantRead' 'HMS_Rcp_QuicConnStreamShutdown' \
  'nghttp3_conn_read_stream2' 'FIELD(":authority", "")' \
  's->requests >= 8' 'if (s->count == H3_SLOTS)' \
  'HTTP/3 response received; QUIC active' 'close unconfirmed; transport disabled'; do
  grep -Fq "$marker" "$SRC" || { echo "Missing HTTP/3 invariant: $marker" >&2; exit 1; }
done
grep -Fq 'ff_ohos_http3_observe(s->location, p)' "$FFMPEG_SOURCE/libavformat/http.c"
grep -Fq 's->off = request_off' "$FFMPEG_SOURCE/libavformat/http.c"
grep -Fq 'ohos_http3_deps="pthreads libdl mbedtls https_protocol"' "$FFMPEG_SOURCE/configure"
"${PYTHON_FOR_BUILD:-python3}" "$ROOT/tests/http3-link-dependencies.py" "$FFMPEG_SOURCE"
if grep -Eq -- '-lrcp(_quic)?(_c)?([[:space:]]|$)' "$ROOT/scripts/ffmpeg.sh"; then
  echo 'RCP QUIC must not be directly linked' >&2; exit 1
fi
echo 'Optional HTTP/3 source invariants passed'
