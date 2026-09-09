#!/bin/bash

set -eu

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)
. "$ROOT_DIR/download/deps-version.sh"
SDK_EXPECTED_VERSION=

mkdir -p ./libmpv

if [ "$(uname -s)" = "Linux" ]; then
  if [ ! -d /sdk ]; then
    echo "Downloading OpenHarmony SDK..."
    ./download/download-sdk.sh
  fi
  SDK_EXPECTED_VERSION=$V_SDK_NATIVE_VERSION
  if [ -d /sdk/ohos-sdk/linux ]; then
    NDK_ROOT=/sdk/ohos-sdk/linux
  else
    NDK_ROOT=/sdk/linux
  fi
elif [ "$(uname -s)" = "Darwin" ]; then
  echo "Using DevEco Studio for macOS, please make sure DevEco Studio is installed."
  NDK_ROOT=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony
else
  echo "Unsupported platform." >&2
  exit 1
fi

. "$ROOT_DIR/env.sh"
mkdir -p "$DEST"
sed -e "s|@OHOS_NDK_HOME@|$NDK_ROOT|g" \
    -e "s|@TARGET_TRIPLE@|$TARGET_TRIPLE|g" \
    -e "s|@CPU_FAMILY@|$FFMPEG_ARCH|g" \
    -e "s|@CPU@|$OHOS_ARCH|g" \
    "$ROOT_DIR/crossfiles/ohos-crossfile.ini.in" > "$CROSS_FILE"

bash "$ROOT_DIR/scripts/verify-sdk.sh" "$NDK_ROOT/native" "$SDK_EXPECTED_VERSION"

./download/download-ohos-rs.sh
./download/download-deps.sh
