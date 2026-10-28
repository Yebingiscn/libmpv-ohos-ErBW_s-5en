#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/dav1d

if [ "$1" == "build" ]; then
	echo -e "\nBuilding dav1d..."
elif [ "$1" == "clean" ]; then
	rm -rf .build
	exit 0
else
	exit 1
fi

prepare_lto_build dav1d "$PWD/.build"
cd .build

meson setup .. \
  --cross-file "$CROSS_FILE" \
  --prefix=$DEST \
  "${MESON_LTO_ARGS[@]}" \
  -Denable_tests=false \
  -Denable_tools=false
ninja -j$CORES
ninja install

popd
