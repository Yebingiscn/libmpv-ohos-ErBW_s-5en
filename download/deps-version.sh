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
V_MBEDTLS=3.6.7
V_DAV1D=1.5.4

# AVS1-P16 (AVS+) software decoder imported into FFmpeg's CAVS decoder.
# Keep this pinned because the upstream integration is distributed as a patch.
V_CAVS_DRA=abae276fed97ce08928f25c8f5e03fd915687f54

# Optical media
V_LIBDVDCSS=1.6.0
V_LIBDVDREAD=7.1.1
V_LIBDVDNAV=7.0.0
V_LIBBLURAY=1.5.0
V_LIBARCHIVE=v3.8.9

# fontconfig
V_LIBXML2=v2.15.3

# libass
V_FRIBIDI=v1.0.16
V_FREETYPE=VER-2-14-3
V_HARFBUZZ=14.3.0
V_FONTCONFIG=2.18.2

# libplacebo
V_DOVI_TOOLS=2.3.3
V_LCMS=lcms2.19.1
V_SHADERC=v2026.3

# mpv and its tightly-coupled dependencies. The OHCodec, zero-copy and Audio
# Vivid patches are rebased on FFmpeg 8.1.2. The OHOS mpv 0.41 branch accepts
# generic Lua only below 5.3, so keep the final Lua 5.2 release for now.
V_FFMPEG=n8.1.2
V_LIBASS=0.17.5
V_LIBPLACEBO=v7.360.1
V_LUA=5.2.4
V_MPV=feat-ohos-0.41.0
V_MPV_COMMIT=6edeee00a07b9b76f197aa71eee3d029fb090de4
