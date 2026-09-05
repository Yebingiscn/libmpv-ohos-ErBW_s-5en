#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
. "$ROOT_DIR/env.sh"
CHECK_DIR="$ROOT_DIR/libmpv/vpe-test"
mkdir -p "$CHECK_DIR"
# Run failure-injection tests on the build host, using real SDK declarations.
"$OHOS_NDK_HOME/native/llvm/bin/clang" --target="$(cc -dumpmachine)" \
  -std=c11 -Wno-ignored-attributes \
  -I"$ROOT_DIR/tests/vpe/mock" -I"$ROOT_DIR/libmpv/mpv" \
  -idirafter "$OHOS_NDK_HOME/native/sysroot/usr/include" \
  "$ROOT_DIR/tests/vpe/lifecycle.c" \
  "$ROOT_DIR/libmpv/mpv/video/out/ohos_vpe.c" \
  -o "$CHECK_DIR/lifecycle"
"$CHECK_DIR/lifecycle"
