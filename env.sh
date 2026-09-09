#!/bin/bash

set -eu

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)

if [ "$(uname -s)" = "Linux" ]; then
  if [ -d /sdk/ohos-sdk/linux ]; then
    export OHOS_NDK_HOME=/sdk/ohos-sdk/linux
  else
    export OHOS_NDK_HOME=/sdk/linux
  fi
  export CORES=$(nproc)
elif [ "$(uname -s)" = "Darwin" ]; then
  export OHOS_NDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony
  export CORES=$(sysctl -n hw.ncpu)
fi

# TARGET_ARCH is consumed by GNU Make's built-in C rules as compiler flags.
# Keep our architecture selector separate from toolchain-owned variables.
export MPV_BUILD_ARCH=${MPV_BUILD_ARCH:-arm64}
case "$MPV_BUILD_ARCH" in
  arm64)
    export TARGET_TRIPLE=aarch64-linux-ohos RUST_TARGET=aarch64-unknown-linux-ohos
    export OHOS_ARCH=arm64-v8a FFMPEG_ARCH=aarch64 FFMPEG_CPU=armv8-a
    export ELF_MACHINE=AArch64 ARCHIVE_ARCH=aarch64
    ;;
  x86_64)
    export TARGET_TRIPLE=x86_64-linux-ohos RUST_TARGET=x86_64-unknown-linux-ohos
    export OHOS_ARCH=x86_64 FFMPEG_ARCH=x86_64 FFMPEG_CPU=x86-64
    export ELF_MACHINE='Advanced Micro Devices X86-64' ARCHIVE_ARCH=x86_64
    ;;
  *) echo "Unsupported MPV_BUILD_ARCH: $MPV_BUILD_ARCH" >&2; exit 1 ;;
esac
export DEST=$ROOT_DIR/libmpv/$MPV_BUILD_ARCH-build
export CROSS_FILE=$ROOT_DIR/libmpv/$MPV_BUILD_ARCH-crossfile.ini
export PATH=$OHOS_NDK_HOME/native/build-tools/cmake/bin:$PATH
export PKG_CONFIG_PATH=$DEST/lib/pkgconfig
export PKG_CONFIG_LIBDIR=$OHOS_NDK_HOME/native/sysroot/usr/lib
export PKG_CONFIG_INCLUDEDIR=$OHOS_NDK_HOME/native/sysroot/usr/include

export AS=$OHOS_NDK_HOME/native/llvm/bin/llvm-as
export CC="$OHOS_NDK_HOME/native/llvm/bin/clang --target=$TARGET_TRIPLE --sysroot=$OHOS_NDK_HOME/native/sysroot"
export CXX="$OHOS_NDK_HOME/native/llvm/bin/clang++ --target=$TARGET_TRIPLE --sysroot=$OHOS_NDK_HOME/native/sysroot"
export LD=$OHOS_NDK_HOME/native/llvm/bin/ld.lld
export STRIP=$OHOS_NDK_HOME/native/llvm/bin/llvm-strip
export RANLIB=$OHOS_NDK_HOME/native/llvm/bin/llvm-ranlib
export OBJDUMP=$OHOS_NDK_HOME/native/llvm/bin/llvm-objdump
export OBJCOPY=$OHOS_NDK_HOME/native/llvm/bin/llvm-objcopy
export NM=$OHOS_NDK_HOME/native/llvm/bin/llvm-nm
export READELF=$OHOS_NDK_HOME/native/llvm/bin/llvm-readelf
export STRINGS=$OHOS_NDK_HOME/native/llvm/bin/llvm-strings
export AR=$OHOS_NDK_HOME/native/llvm/bin/llvm-ar
export CFLAGS='-fPIC -D__MUSL__=1'
export CXXFLAGS='-fPIC -D__MUSL__=1'
