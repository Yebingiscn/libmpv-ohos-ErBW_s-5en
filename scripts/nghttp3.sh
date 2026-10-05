#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
. "$ROOT_DIR/env.sh"
test "${1:-}" = build
cmake -S "$ROOT_DIR/libmpv/nghttp3" -B "$ROOT_DIR/libmpv/nghttp3/.build" \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$FFMPEG_ARCH" \
  -DCMAKE_C_COMPILER="$OHOS_NDK_HOME/native/llvm/bin/clang" \
  -DCMAKE_C_COMPILER_TARGET="$TARGET_TRIPLE" \
  -DCMAKE_SYSROOT="$OHOS_NDK_HOME/native/sysroot" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_INSTALL_PREFIX="$DEST" -DCMAKE_INSTALL_LIBDIR=lib \
  -DENABLE_LIB_ONLY=ON -DENABLE_STATIC_LIB=ON -DENABLE_SHARED_LIB=OFF \
  -DBUILD_TESTING=OFF
cmake --build "$ROOT_DIR/libmpv/nghttp3/.build" -j "$CORES"
cmake --install "$ROOT_DIR/libmpv/nghttp3/.build"
