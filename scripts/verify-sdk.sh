#!/bin/bash

set -eu

SDK_NATIVE=${1:?Usage: verify-sdk.sh NATIVE_SDK_DIR [EXPECTED_VERSION]}

python3 - "$SDK_NATIVE/oh-uni-package.json" "${2:-}" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as metadata:
    package = json.load(metadata)
version = package.get("version", "unknown")
print(f"Native SDK: {version}, API {package.get('apiVersion', 'unknown')}")
if int(package.get("apiVersion", 0)) < 26:
    sys.exit("SDK API 26 or newer is required")
if sys.argv[2] and version != sys.argv[2]:
    sys.exit(f"Unexpected SDK version {version}; expected {sys.argv[2]}")
PY

API26_HEADER=$SDK_NATIVE/sysroot/usr/include/multimedia/player_framework/native_avcodec_base.h
if ! grep -q 'typedef enum OH_FrameRetentionMode' "$API26_HEADER" ||
   ! grep -q 'OH_MD_KEY_VIDEO_DECODER_SPEED' "$API26_HEADER"; then
  echo "SDK is missing required API 26 AVCodec declarations" >&2
  exit 1
fi

SUITE_HEADER=$SDK_NATIVE/sysroot/usr/include/ohaudiosuite/native_audio_suite_base.h
if ! grep -q 'EFFECT_NODE_TYPE_HOA_SPACE_RENDER[[:space:]]*=' "$SUITE_HEADER"; then
  echo "SDK is missing AudioSuite HOA; refusing to build with HOA disabled" >&2
  exit 1
fi

echo "SDK AVCodec and AudioSuite HOA checks passed"
