#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")"; pwd)

. "$ROOT_DIR/env.sh"

LIBMPV=$DEST/libmpv.so

if [ ! -f "$LIBMPV" ]; then
  echo "Missing build output: $LIBMPV" >&2
  exit 1
fi

features=$("$STRINGS" "$LIBMPV" | grep "^List of enabled features:")
configuration=$("$STRINGS" "$LIBMPV" | grep "^Configuration:")

for feature in dvdnav libarchive libbluray ohos; do
  if ! grep -Eq "(^| )$feature( |$)" <<< "$features"; then
    echo "Missing mpv feature: $feature" >&2
    exit 1
  fi
done

if ! grep -q -- "-Dgpl=true" <<< "$configuration"; then
  echo "mpv was not built with GPL support enabled" >&2
  exit 1
fi

symbols=$("$NM" -D --defined-only "$LIBMPV")
for symbol in mpv_create; do
  if ! grep -Eq "[[:space:]]$symbol$" <<< "$symbols"; then
    echo "Missing required dynamic symbol: $symbol" >&2
    exit 1
  fi
done

dynamic_section=$("$READELF" -d "$LIBMPV")
if grep -Eq "NEEDED.*lib(dvd|bluray|archive)" <<< "$dynamic_section"; then
  echo "Optical-media dependencies must be linked statically" >&2
  exit 1
fi

# FreeType is built before HarfBuzz so its optional auto-hinter integration
# must stay disabled.  Otherwise FreeType emits a runtime dlopen probe for
# libharfbuzz.so.0 on every shaping operation, even though libass links the
# later HarfBuzz build statically.
if "$STRINGS" "$LIBMPV" | grep -Fxq "libharfbuzz.so.0"; then
  echo "FreeType runtime HarfBuzz probing is still enabled" >&2
  exit 1
fi

for marker in vvc_ohcodec video/vvc vvc_mp4toannexb; do
  if ! "$STRINGS" "$LIBMPV" | grep -Fx "$marker" >/dev/null; then
    echo "Missing VVC OHCodec marker: $marker" >&2
    exit 1
  fi
done

# Optional OHCodec metadata keys are resolved with dlsym at runtime so the
# same libmpv remains loadable on older HarmonyOS releases.
undefined_symbols=$("$NM" -D --undefined-only "$LIBMPV")
for symbol in OH_MD_KEY_VIDEO_DECODER_OUTPUT_ENABLE_VRR \
              OH_MD_KEY_VIDEO_DECODER_FRAME_RETENTION_MODE \
              OH_MD_KEY_VIDEO_DECODER_SPEED; do
  if grep -Eq "[[:space:]]$symbol$" <<< "$undefined_symbols"; then
    echo "OHCodec optional key must not be hard-linked: $symbol" >&2
    exit 1
  fi
done

for marker in OH_MD_KEY_VIDEO_DECODER_OUTPUT_ENABLE_VRR \
              OH_MD_KEY_VIDEO_DECODER_FRAME_RETENTION_MODE \
              OH_MD_KEY_VIDEO_DECODER_SPEED; do
  if ! "$STRINGS" "$LIBMPV" | grep -Fx "$marker" >/dev/null; then
    echo "Missing OHCodec runtime key marker: $marker" >&2
    exit 1
  fi
done

if ! "$STRINGS" "$LIBMPV" | grep -F "[OHCodecPolicy]" >/dev/null; then
  echo "Missing OHCodec playback-policy diagnostic marker" >&2
  exit 1
fi

echo "$features"
echo "$configuration"
grep -E "NEEDED|SONAME" <<< "$dynamic_section"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$LIBMPV"
else
  shasum -a 256 "$LIBMPV"
fi
