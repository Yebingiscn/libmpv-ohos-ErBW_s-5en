#!/bin/bash
# Build only the host decoders needed for byte-exact regression tests. This is
# separate from the target build and must never reuse target configure caches.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
FFMPEG_SOURCE=${FFMPEG_SOURCE:-$ROOT/libmpv/ffmpeg}
# Also works when invoked from a shell that previously sourced env.sh.
unset CC CXX LD AS AR NM RANLIB STRIP CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_INCLUDEDIR
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
cd "$TMP_DIR"
"$FFMPEG_SOURCE/configure" --disable-everything --disable-autodetect \
  --disable-asm --disable-doc --disable-programs --disable-avdevice \
  --disable-avfilter --disable-swscale --disable-swresample --disable-avformat \
  --enable-decoder=dst,wavpack --cc="${CC_FOR_BUILD:-cc}" >configure.log 2>&1 || {
    cat configure.log; exit 1;
  }
make -j4 >build.log 2>&1 || { tail -100 build.log; exit 1; }
"${CC_FOR_BUILD:-cc}" -std=c11 -I. -I"$FFMPEG_SOURCE" \
  "$ROOT/tests/dsd-decoders.c" libavcodec/libavcodec.a libavutil/libavutil.a \
  -lm -pthread -o dsd-decoders
"$TMP_DIR/dsd-decoders" "$ROOT/tests/fixtures/dsd/pattern-fast.wv" \
  "$ROOT/tests/fixtures/dsd/pattern-high.wv" "$ROOT/tests/fixtures/dsd/pattern.raw" \
  "$ROOT/tests/fixtures/dsd/pattern-pcm.wv"
