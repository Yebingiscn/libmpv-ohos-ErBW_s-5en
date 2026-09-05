#!/bin/bash

set -eu

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)
. "$ROOT_DIR/download/deps-version.sh"
SDK_EXPECTED_VERSION=

mkdir -p ./libmpv/arm64-build

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
  sed "s|@OHOS_NDK_HOME@|$NDK_ROOT|g" \
    ./crossfiles/arm64-crossfile-linux.ini > ./libmpv/arm64-crossfile.ini
elif [ "$(uname -s)" = "Darwin" ]; then
  echo "Using DevEco Studio for macOS, please make sure DevEco Studio is installed."
  NDK_ROOT=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony
  ln -sf ../crossfiles/arm64-crossfile-macos.ini ./libmpv/arm64-crossfile.ini
else
  echo "Unsupported platform." >&2
  exit 1
fi

bash "$ROOT_DIR/scripts/verify-sdk.sh" "$NDK_ROOT/native" "$SDK_EXPECTED_VERSION"

./download/download-ohos-rs.sh
./download/download-deps.sh
