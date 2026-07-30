#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh

pushd $ROOT_DIR/libmpv/libdvdcss

if [ "$1" == "build" ]; then
  echo -e "\nBuilding libdvdcss..."
elif [ "$1" == "clean" ]; then
  rm -rf .build
  exit 0
else
  exit 1
fi

if [ ! -f configure ]; then
  autoreconf -fi
fi

mkdir -p .build
cd .build

../configure \
  --host=aarch64-linux-musl \
  --prefix=$DEST \
  --disable-shared \
  --enable-static \
  --disable-doc
make -j$CORES
make install

popd
