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
archive="libmpv_$ARCHIVE_ARCH.zip"
stage=$(mktemp -d "$DEST/.bundle.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT
mkdir "$stage/include"
for headers in ass libavcodec libavfilter libavformat libavutil libplacebo libswresample libswscale; do
  cp -R "$DEST/include/$headers" "$stage/include/"
done
cp -R "$DEST/mpv/include/mpv" "$stage/include/"
ln "$DEST/libmpv.so" "$stage/libmpv.so"
cp "$DEST/build-optimization.json" "$stage/"
(cd "$stage"; zip -qr "$archive" libmpv.so build-optimization.json include)
mv -f "$stage/$archive" "$DEST/$archive"
