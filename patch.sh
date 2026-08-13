#!/bin/bash

set -eu

PATCHES=(patches/*)
ROOT=$(pwd)

# The public AVS+ integration also contains an unrelated DRA decoder. Import
# only the three libcavs files, then let our FFmpeg 8.1 patch wire them into
# the build and apply the OpenHarmony portability fixes.
CAVS_PATCH=$ROOT/libmpv/ffmpeg_cavs_dra/ffmpeg-7.1.2_cavs_dra.patch
if [ ! -f "$CAVS_PATCH" ]; then
  echo "Missing pinned AVS+ decoder patch: $CAVS_PATCH" >&2
  exit 1
fi

echo "Importing AVS+ decoder source into FFmpeg..."
git -C ./libmpv/ffmpeg apply -p2 --recount \
  --ignore-space-change --ignore-whitespace \
  --include=libavcodec/libcavs.c \
  --include=libavcodec/libcavs.h \
  --include=libavcodec/libcavsdec.c \
  "$CAVS_PATCH"

for dep_path in "${PATCHES[@]}"; do
  if [ -d "$dep_path" ]; then
    patches=($dep_path/*)
    dep=${dep_path#*/}
    pushd ./libmpv/$dep
    echo "Patching $dep..."
    for patch in "${patches[@]}"; do
      echo "Applying $patch..."
      git apply "$ROOT/$patch"
    done
    popd
  fi
done
