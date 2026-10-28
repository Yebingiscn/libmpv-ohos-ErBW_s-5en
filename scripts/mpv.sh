#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/mpv

if [ "$1" == "build" ]; then
	echo -e "\nBuilding mpv..."
elif [ "$1" == "clean" ]; then
	rm -rf .build
	exit 0
else
	exit 1
fi

prepare_lto_build mpv "$PWD/.build"
cd .build

meson setup .. \
  --cross-file "$CROSS_FILE" \
  --prefix=$DEST/mpv \
  --default-library shared \
  --strip \
  "${MPV_LTO_ARGS[@]}" \
  -Dopensles=disabled \
  -Dohos=enabled \
  -Degl-ohos=enabled \
  -Dvulkan=enabled \
  -Dshaderc=enabled \
  -Dlua=enabled \
  -Dgpl=true \
  -Ddvdnav=enabled \
  -Dlibbluray=enabled \
  -Dlibarchive=enabled \
  -Dbuild-date=false \
  -Dcplayer=false \
  -Dmanpage-build=disabled
ninja -j$CORES
ninja install

cd $DEST/mpv/lib
mv libmpv.so ../../libmpv.so

popd
