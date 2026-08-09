#!/bin/bash

set -eu

. ./download/deps-version.sh

pushd /

sudo wget -qO sdk.tar.gz https://repo.huaweicloud.com/$V_SDK_REPOSITORY/os/$V_SDK/ohos-sdk-windows_linux-public.tar.gz
sudo mkdir -p sdk
sudo tar -C sdk -zxf sdk.tar.gz
sudo rm sdk.tar.gz

cd sdk
sudo rm -rf windows/ ohos-sdk/windows/

# Extract NDK
if [ -d linux ]; then
  SDK_HOST_DIR=/sdk/linux
elif [ -d ohos-sdk/linux ]; then
  SDK_HOST_DIR=/sdk/ohos-sdk/linux
else
  echo "Downloaded SDK has no Linux host directory" >&2
  find /sdk -maxdepth 3 -type d -print >&2
  exit 1
fi

cd "$SDK_HOST_DIR"
for i in *.zip
do
  sudo unzip -q $i
  sudo rm $i
done

API26_HEADER=$SDK_HOST_DIR/native/sysroot/usr/include/multimedia/player_framework/native_avcodec_base.h
if ! grep -q 'typedef enum OH_FrameRetentionMode' "$API26_HEADER" ||
   ! grep -q 'OH_MD_KEY_VIDEO_DECODER_SPEED' "$API26_HEADER"; then
  echo "Downloaded SDK does not provide the required API 26 AVCodec declarations" >&2
  exit 1
fi

popd
