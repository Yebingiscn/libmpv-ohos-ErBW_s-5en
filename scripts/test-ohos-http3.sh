#!/bin/bash
# Host-only protocol tests. Never reuse the target's configure/build caches.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.."; pwd)
FFMPEG_SOURCE=${FFMPEG_SOURCE:-$ROOT/libmpv/ffmpeg}
NGHTTP3_SOURCE=${NGHTTP3_SOURCE:-$ROOT/libmpv/nghttp3}
MBEDTLS_SOURCE=${MBEDTLS_SOURCE:-$ROOT/libmpv/mbedtls}
CC_FOR_BUILD=${CC_FOR_BUILD:-cc}
unset CC CXX LD AS AR NM RANLIB STRIP CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_INCLUDEDIR
TMP_DIR=$(mktemp -d)
report_failure() {
  local log
  for log in "$TMP_DIR/nghttp3.log" "$TMP_DIR/mbedtls.log" \
             "$TMP_DIR/ffmpeg/configure.log" "$TMP_DIR/ffmpeg/build.log"; do
    if [ -f "$log" ]; then
      printf '\nHTTP/3 test diagnostic: %s\n' "$log" >&2
      tail -n 80 "$log" >&2
    fi
  done
}
trap report_failure ERR
trap 'rm -rf -- "$TMP_DIR"' EXIT
cmake -S "$NGHTTP3_SOURCE" -B "$TMP_DIR/nghttp3" \
  -DCMAKE_C_COMPILER="$CC_FOR_BUILD" -DENABLE_LIB_ONLY=ON \
  -DENABLE_SHARED_LIB=OFF -DBUILD_TESTING=OFF >"$TMP_DIR/nghttp3.log" 2>&1
cmake --build "$TMP_DIR/nghttp3" -j4 >>"$TMP_DIR/nghttp3.log" 2>&1
cmake -S "$MBEDTLS_SOURCE" -B "$TMP_DIR/mbedtls" \
  -DCMAKE_C_COMPILER="$CC_FOR_BUILD" -DENABLE_PROGRAMS=OFF \
  -DENABLE_TESTING=OFF >"$TMP_DIR/mbedtls.log" 2>&1
cmake --build "$TMP_DIR/mbedtls" -j4 >>"$TMP_DIR/mbedtls.log" 2>&1
mkdir "$TMP_DIR/ffmpeg"
cd "$TMP_DIR/ffmpeg"
"$FFMPEG_SOURCE/configure" --disable-everything --disable-autodetect \
  --disable-asm --disable-doc --disable-programs --disable-avdevice \
  --disable-avfilter --disable-swscale --disable-swresample \
  --enable-protocol=http --enable-network --cc="$CC_FOR_BUILD" >configure.log 2>&1
make -j4 libavutil/libavutil.a >build.log 2>&1
"$CC_FOR_BUILD" -std=c17 -ffunction-sections -fdata-sections -I. -I"$FFMPEG_SOURCE" \
  -c "$FFMPEG_SOURCE/libavformat/utils.c" -o utils.o
"$CC_FOR_BUILD" -std=c17 -I. -I"$FFMPEG_SOURCE" -include "$ROOT/tests/http3-enable.h" \
  -Werror=implicit-function-declaration -c "$FFMPEG_SOURCE/libavformat/http.c" -o http.o
"$CC_FOR_BUILD" -std=c17 -DNGHTTP3_STATICLIB -ffunction-sections -fdata-sections \
  -Werror=implicit-function-declaration -I. -I"$FFMPEG_SOURCE" \
  -I"$NGHTTP3_SOURCE/lib/includes" -I"$TMP_DIR/nghttp3/lib/includes" \
  -I"$MBEDTLS_SOURCE/include" "$ROOT/tests/ohos-http3.c" utils.o \
  libavutil/libavutil.a "$TMP_DIR/nghttp3/lib/libnghttp3.a" \
  "$TMP_DIR/mbedtls/library/libmbedx509.a" "$TMP_DIR/mbedtls/library/libmbedcrypto.a" \
  -Wl,--gc-sections -ldl -pthread -lm -o "$TMP_DIR/http3-test"
"$TMP_DIR/http3-test"
"$TMP_DIR/http3-test" missing-library
"$TMP_DIR/http3-test" missing-symbol
