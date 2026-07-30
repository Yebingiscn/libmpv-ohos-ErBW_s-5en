#!/bin/bash

set -eu

# ffmpeg
./scripts/mbedtls.sh build
./scripts/dav1d.sh build
./scripts/ffmpeg.sh build

# DVD
./scripts/libdvdcss.sh build
./scripts/libdvdread.sh build
./scripts/libdvdnav.sh build

# fontconfig
./scripts/libxml2.sh build

# libass
./scripts/fribidi.sh build
./scripts/freetype.sh build
./scripts/harfbuzz.sh build
./scripts/fontconfig.sh build
./scripts/libass.sh build

# Blu-ray and archive support
./scripts/libbluray.sh build
./scripts/libarchive.sh build

# libplacebo
./scripts/dovi_tools.sh build
./scripts/lcms.sh build
./scripts/shaderc.sh build
./scripts/libplacebo.sh build

# mpv
./scripts/lua.sh build
./scripts/mpv.sh build
