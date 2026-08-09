#!/bin/bash

set -eu

. ./download/deps-version.sh

pushd /

sudo wget -qO sdk.tar.gz https://repo.huaweicloud.com/$V_SDK_REPOSITORY/os/$V_SDK/ohos-sdk-windows_linux-public.tar.gz
sudo mkdir -p sdk
sudo tar -C sdk -zxf sdk.tar.gz
sudo rm sdk.tar.gz

cd sdk
sudo rm -rf windows/

# Extract NDK
cd linux
for i in *.zip
do
  sudo unzip -q $i
  sudo rm $i
done

API26_HEADER=/sdk/linux/native/sysroot/usr/include/multimedia/player_framework/native_avcodec_base.h
if ! grep -q 'typedef enum OH_FrameRetentionMode' "$API26_HEADER" ||
   ! grep -q 'OH_MD_KEY_VIDEO_DECODER_SPEED' "$API26_HEADER"; then
  echo "Downloaded SDK does not provide the required API 26 AVCodec declarations" >&2
  exit 1
fi

popd
