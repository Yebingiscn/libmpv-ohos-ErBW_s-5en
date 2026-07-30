#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh

pushd $ROOT_DIR/libmpv/libarchive

if [ "$1" == "build" ]; then
  echo -e "\nBuilding libarchive..."
elif [ "$1" == "clean" ]; then
  rm -rf .build
  exit 0
else
  exit 1
fi

mkdir -p .build
cd .build

cmake -L \
  -DCMAKE_TOOLCHAIN_FILE=$OHOS_NDK_HOME/native/build/cmake/ohos.toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX=$DEST \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SKIP_RPATH=TRUE \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_WERROR=OFF \
  -DENABLE_OPENSSL=OFF \
  -DENABLE_MBEDTLS=OFF \
  -DENABLE_NETTLE=OFF \
  -DENABLE_LIBB2=OFF \
  -DENABLE_LZ4=OFF \
  -DENABLE_LZO=OFF \
  -DENABLE_LZMA=OFF \
  -DENABLE_ZSTD=OFF \
  -DENABLE_ZLIB=ON \
  -DENABLE_BZip2=OFF \
  -DENABLE_LIBXML2=OFF \
  -DENABLE_EXPAT=OFF \
  -DENABLE_WIN32_XMLLITE=OFF \
  -DENABLE_PCREPOSIX=OFF \
  -DENABLE_PCRE2POSIX=OFF \
  -DENABLE_LIBGCC=OFF \
  -DENABLE_CNG=OFF \
  -DENABLE_TAR=OFF \
  -DENABLE_CPIO=OFF \
  -DENABLE_CAT=OFF \
  -DENABLE_UNZIP=OFF \
  -DENABLE_XATTR=OFF \
  -DENABLE_ACL=OFF \
  -DENABLE_ICONV=OFF \
  -DENABLE_TEST=OFF \
  -DENABLE_INSTALL=ON \
  -DOHOS_STL=c++_shared \
  -GNinja \
  ..
ninja -j$CORES
ninja install

popd
