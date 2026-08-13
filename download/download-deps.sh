#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/download/deps-version.sh

pushd $ROOT_DIR/libmpv

# mbedtls
if [ ! -d mbedtls ]; then
  echo "Downloading mbedtls..."
	mkdir mbedtls
	wget -qO mbedtls.tar.bz2 https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$V_MBEDTLS/mbedtls-$V_MBEDTLS.tar.bz2
  tar -C mbedtls --strip-components=1 -jxf mbedtls.tar.bz2
  rm mbedtls.tar.bz2
else
  echo "mbedtls already exists, skipping."
fi

# dav1d
if [ ! -d dav1d ]; then
  echo "Downloading dav1d..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_DAV1D https://code.videolan.org/videolan/dav1d.git dav1d > /dev/null
else
  echo "dav1d already exists, skipping."
fi

# libdvdcss
if [ ! -d libdvdcss ]; then
  echo "Downloading libdvdcss..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBDVDCSS https://code.videolan.org/videolan/libdvdcss.git libdvdcss > /dev/null
else
  echo "libdvdcss already exists, skipping."
fi

# libdvdread
if [ ! -d libdvdread ]; then
  echo "Downloading libdvdread..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBDVDREAD https://code.videolan.org/videolan/libdvdread.git libdvdread > /dev/null
else
  echo "libdvdread already exists, skipping."
fi

# libdvdnav
if [ ! -d libdvdnav ]; then
  echo "Downloading libdvdnav..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBDVDNAV https://code.videolan.org/videolan/libdvdnav.git libdvdnav > /dev/null
else
  echo "libdvdnav already exists, skipping."
fi

# libbluray
if [ ! -d libbluray ]; then
  echo "Downloading libbluray..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBBLURAY --recursive https://code.videolan.org/videolan/libbluray.git libbluray > /dev/null
else
  echo "libbluray already exists, skipping."
fi

# libarchive
if [ ! -d libarchive ]; then
  echo "Downloading libarchive..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBARCHIVE https://github.com/libarchive/libarchive.git libarchive > /dev/null
else
  echo "libarchive already exists, skipping."
fi

# libxml2
if [ ! -d libxml2 ]; then
  echo "Downloading libxml2..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBXML2 --recursive https://gitlab.gnome.org/GNOME/libxml2.git libxml2 > /dev/null
else
  echo "libxml2 already exists, skipping."
fi

# fribidi
if [ ! -d fribidi ]; then
  echo "Downloading fribidi..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_FRIBIDI https://github.com/fribidi/fribidi.git fribidi > /dev/null
else
  echo "fribidi already exists, skipping."
fi

# freetype
if [ ! -d freetype ]; then
  echo "Downloading freetype..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_FREETYPE https://gitlab.freedesktop.org/freetype/freetype.git freetype > /dev/null
else
  echo "freetype already exists, skipping."
fi

# harfbuzz
if [ ! -d harfbuzz ]; then
  echo "Downloading harfbuzz..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_HARFBUZZ https://github.com/harfbuzz/harfbuzz.git harfbuzz > /dev/null
else
  echo "harfbuzz already exists, skipping."
fi

# fontconfig
if [ ! -d fontconfig ]; then
  echo "Downloading fontconfig..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_FONTCONFIG https://gitlab.freedesktop.org/fontconfig/fontconfig.git fontconfig > /dev/null
else
  echo "fontconfig already exists, skipping."
fi

# dovi_tools
if [ ! -d dovi_tools ]; then
  echo "Downloading dovi_tools..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_DOVI_TOOLS https://github.com/quietvoid/dovi_tool.git dovi_tools > /dev/null
else
  echo "dovi_tools already exists, skipping."
fi

# lcms
if [ ! -d lcms ]; then
  echo "Downloading lcms..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LCMS https://github.com/mm2/Little-CMS.git lcms > /dev/null
else
  echo "lcms already exists, skipping."
fi

# shaderc
if [ ! -d shaderc ]; then
  echo "Downloading shaderc..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_SHADERC https://github.com/google/shaderc.git shaderc > /dev/null
  cd shaderc
  ./utils/git-sync-deps > /dev/null
  cd ..
else
  echo "shaderc already exists, skipping."
fi

# ffmpeg
if [ ! -d ffmpeg ]; then
  echo "Downloading ffmpeg..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_FFMPEG https://code.ffmpeg.org/FFmpeg/FFmpeg.git ffmpeg > /dev/null
else
  echo "ffmpeg already exists, skipping."
fi

# AVS+ decoder source. Only the libcavs video files are imported by patch.sh;
# the unrelated DRA audio decoder in this repository is deliberately omitted.
if [ ! -d ffmpeg_cavs_dra ]; then
  echo "Downloading AVS+ decoder source..."
  git init -q ffmpeg_cavs_dra
  git -C ffmpeg_cavs_dra remote add origin https://github.com/maliwen2015/ffmpeg_cavs_dra.git
else
  echo "AVS+ decoder source already exists, checking pinned revision."
fi

actual_cavs_dra_commit=$(git -C ffmpeg_cavs_dra rev-parse HEAD 2>/dev/null || true)
if [ "$actual_cavs_dra_commit" != "$V_CAVS_DRA" ]; then
  git -C ffmpeg_cavs_dra fetch -q --depth 1 origin "$V_CAVS_DRA"
  git -C ffmpeg_cavs_dra -c advice.detachedHead=false checkout -q "$V_CAVS_DRA"
fi

actual_cavs_dra_commit=$(git -C ffmpeg_cavs_dra rev-parse HEAD)
if [ "$actual_cavs_dra_commit" != "$V_CAVS_DRA" ]; then
  echo "Unexpected AVS+ source commit: $actual_cavs_dra_commit (expected $V_CAVS_DRA)" >&2
  exit 1
fi

# libass
if [ ! -d libass ]; then
  echo "Downloading libass..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBASS https://github.com/libass/libass.git libass > /dev/null
else
  echo "libass already exists, skipping."
fi

# libplacebo
if [ ! -d libplacebo ]; then
  echo "Downloading libplacebo..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_LIBPLACEBO --recursive https://code.videolan.org/videolan/libplacebo.git libplacebo > /dev/null
else
  echo "libplacebo already exists, skipping."
fi

# lua
if [ ! -d lua ]; then
  echo "Downloading lua..."
  mkdir lua
  wget -qO lua.tar.gz https://www.lua.org/ftp/lua-$V_LUA.tar.gz
  tar -C lua --strip-components=1 -zxf lua.tar.gz
  rm lua.tar.gz
else
  echo "lua already exists, skipping."
fi

# mpv
if [ ! -d mpv ]; then
  echo "Downloading mpv..."
  git -c advice.detachedHead=false clone -q --depth 1 -b $V_MPV https://github.com/ErBWs/mpv.git mpv > /dev/null
else
  echo "mpv already exists, skipping."
fi

actual_mpv_commit=$(git -C mpv rev-parse HEAD)
if [ "$actual_mpv_commit" != "$V_MPV_COMMIT" ]; then
  echo "Checking out pinned mpv commit $V_MPV_COMMIT..."
  git -C mpv fetch -q --depth 1 origin "$V_MPV_COMMIT"
  git -C mpv -c advice.detachedHead=false checkout -q "$V_MPV_COMMIT"
fi

actual_mpv_commit=$(git -C mpv rev-parse HEAD)
if [ "$actual_mpv_commit" != "$V_MPV_COMMIT" ]; then
  echo "Unexpected mpv commit: $actual_mpv_commit (expected $V_MPV_COMMIT)" >&2
  exit 1
fi

popd
