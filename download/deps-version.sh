#!/bin/bash

set -eu

# Public API 26 Release NDK. AudioSuite headers match DevEco 26.0.0.105,
# including the HOA node missing from the older Beta1 public package.
V_SDK=7.0-Release
V_SDK_REPOSITORY=harmonyos
V_SDK_ARCHIVE=ohos-sdk-windows_linux-public_20260829.tar.gz
V_SDK_SHA256=6bf6ae1efe8de0e8bd15ddbd7fac58bcb54d9620262a541b9b219439317a4c42
V_SDK_NATIVE_VERSION=26.0.0.38

# FFmpeg TLS/AV1 dependencies. Keep Mbed TLS on the supported 3.6 LTS ABI;
# moving to the 4.x ABI needs a separate FFmpeg adapter validation.
V_MBEDTLS=3.6.8
V_DAV1D=1.5.5
V_NGHTTP3=v1.19.0
V_NGHTTP3_COMMIT=dc83309ca5fd2beb91cc5c030a2d30525e9f2a00

# AVS1-P16 (AVS+) software decoder imported into FFmpeg's CAVS decoder.
# Keep this pinned because the upstream integration is distributed as a patch.
V_CAVS_DRA=abae276fed97ce08928f25c8f5e03fd915687f54

# Optical media
V_LIBDVDCSS=1.6.0
V_LIBDVDREAD=7.1.1
V_LIBDVDNAV=7.0.0
V_LIBBLURAY=1.5.0
V_LIBARCHIVE=v3.8.11

# fontconfig
V_LIBXML2=v2.15.4

# libass
V_FRIBIDI=v1.0.18
V_FREETYPE=VER-2-15-0
V_HARFBUZZ=15.2.0
V_FONTCONFIG=2.18.4

# libplacebo
V_DOVI_TOOLS=2.3.4
V_LCMS=lcms2.20
V_SHADERC=v2026.4

# mpv and its tightly coupled dependencies. Keep Lua on the supported 5.2 ABI.
V_FFMPEG=n9.1
V_LIBASS=0.17.5
V_LIBPLACEBO=v7.387.0
V_LUA=5.2.4
V_MPV=v0.42.0
V_MPV_REPOSITORY=https://github.com/mpv-player/mpv.git
V_MPV_COMMIT=b0841e8c4d156b8403dc1876c4cbd9011eb583f3
