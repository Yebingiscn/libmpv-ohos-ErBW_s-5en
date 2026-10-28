#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/libplacebo

if [ "$1" == "build" ]; then
	echo -e "\nBuilding libplacebo..."
elif [ "$1" == "clean" ]; then
	rm -rf .build
	exit 0
else
	exit 1
fi

prepare_lto_build libplacebo "$PWD/.build"
cd .build

meson setup .. \
  --cross-file "$CROSS_FILE" \
  --prefix=$DEST \
  "${MESON_LTO_ARGS[@]}" \
  -Ddovi=enabled \
  -Dlcms=enabled \
  -Dshaderc=enabled \
  -Dvulkan=enabled \
  -Dopengl=enabled \
  -Ddemos=false
ninja -j$CORES
ninja install

popd
