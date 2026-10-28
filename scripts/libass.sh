#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/libass

if [ "$1" == "build" ]; then
	echo -e "\nBuilding libass..."
elif [ "$1" == "clean" ]; then
	rm -rf .build
	exit 0
else
	exit 1
fi

prepare_lto_build libass "$PWD/.build"
cd .build

meson setup .. \
  --cross-file "$CROSS_FILE" \
  --prefix=$DEST \
  "${MESON_LTO_ARGS[@]}" \
  -Dtest=disabled \
  -Dcompare=disabled \
  -Dprofile=disabled \
  -Dfuzz=disabled \
  -Dfontconfig=enabled
ninja -j$CORES
ninja install

popd
