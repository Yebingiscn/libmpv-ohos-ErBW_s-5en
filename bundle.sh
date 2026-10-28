#!/bin/bash

set -eu

./download.sh
(
  . ./env.sh
  bash "$ROOT_DIR/tests/cmake-lto-abi.sh" "$OHOS_NDK_HOME/native" "$TARGET_TRIPLE"
)
./patch.sh
./build.sh
./verify.sh

. ./env.sh
cd "$DEST"
zip "libmpv_$ARCHIVE_ARCH.zip" libmpv.so build-optimization.json
