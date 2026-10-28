#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
. "$ROOT_DIR/env.sh"
. "$ROOT_DIR/scripts/build-lto.sh"
test "${1:-}" = build
nghttp3_build="$ROOT_DIR/libmpv/nghttp3/.build"
# A previous static-only ABI probe may be cached as successful even though
# its byte order/pointer size are empty. Never delete or silently reuse it.
if [ -f "$nghttp3_build/CMakeCache.txt" ] &&
   grep -Eq '^CMAKE_TRY_COMPILE_TARGET_TYPE:[^=]*=STATIC_LIBRARY[[:space:]]*$' "$nghttp3_build/CMakeCache.txt"; then
  echo 'nghttp3 has cached static-only ABI probes. Use a fresh checkout for ThinLTO.' >&2
  exit 1
fi
prepare_lto_build nghttp3 "$nghttp3_build"
# ThinLTO archives contain IR, not the native ABI/type-size marker strings
# CMake reads. Link its probes to ELF; cross-compilation never executes them.
cmake -S "$ROOT_DIR/libmpv/nghttp3" -B "$nghttp3_build" \
  "${CMAKE_LTO_ARGS[@]}" \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$FFMPEG_ARCH" \
  -DCMAKE_C_COMPILER="$OHOS_NDK_HOME/native/llvm/bin/clang" \
  -DCMAKE_C_COMPILER_TARGET="$TARGET_TRIPLE" \
  -DCMAKE_SYSROOT="$OHOS_NDK_HOME/native/sysroot" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=EXECUTABLE \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_INSTALL_PREFIX="$DEST" -DCMAKE_INSTALL_LIBDIR=lib \
  -DENABLE_LIB_ONLY=ON -DENABLE_STATIC_LIB=ON -DENABLE_SHARED_LIB=OFF \
  -DBUILD_TESTING=OFF
cmake --build "$nghttp3_build" -j "$CORES"
cmake --install "$nghttp3_build"
