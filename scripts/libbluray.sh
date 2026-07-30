#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh

pushd $ROOT_DIR/libmpv/libbluray

if [ "$1" == "build" ]; then
  echo -e "\nBuilding libbluray..."
elif [ "$1" == "clean" ]; then
  rm -rf .build
  exit 0
else
  exit 1
fi

mkdir -p .build
cd .build

meson setup .. \
  --cross-file $ROOT_DIR/libmpv/arm64-crossfile.ini \
  --prefix=$DEST \
  --force-fallback-for=libudfread \
  -Denable_docs=false \
  -Denable_tools=false \
  -Denable_devtools=false \
  -Denable_examples=false \
  -Dbdj_jar=disabled \
  -Dembed_udfread=true \
  -Dfontconfig=enabled \
  -Dfreetype=enabled \
  -Dlibxml2=enabled
ninja -j$CORES
ninja install

popd
