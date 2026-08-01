#!/bin/bash

set -eu

# OpenHarmony SDK version
V_SDK=6.0-Release

# FFmpeg TLS/AV1 dependencies. Keep Mbed TLS on the supported 3.6 LTS ABI;
# moving to the 4.x ABI needs a separate FFmpeg adapter validation.
V_MBEDTLS=3.6.7
V_DAV1D=1.5.4

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

# mpv and its tightly-coupled dependencies. FFmpeg stays on the latest 8.0.x
# release because the OHCodec and Audio Vivid patches target the 8.0 parser and
# decoder internals. The OHOS mpv 0.41 branch accepts generic Lua only below 5.3.
V_FFMPEG=n8.0.3
V_LIBASS=0.17.5
V_LIBPLACEBO=v7.360.1
V_LUA=5.2.4
V_MPV=feat-ohos-0.41.0
V_MPV_COMMIT=6edeee00a07b9b76f197aa71eee3d029fb090de4
