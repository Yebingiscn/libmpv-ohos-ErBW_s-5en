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

# mpv discovers FFmpeg decoders at runtime, so this decoder description is a
# direct check that the AVS1-P16 capable libcavs implementation reached the
# final statically linked libmpv rather than FFmpeg's AVS1-P2-only decoder.
if ! "$STRINGS" "$LIBMPV" | grep -F "AVS1-P16, Guangdian profile" >/dev/null; then
  echo "Missing AVS+ (AVS1-P16) software decoder" >&2
  exit 1
fi

# libcavs' arithmetic decoder decrements bits_to_go below zero to refill its
# byte buffer.  On AArch64, plain char is unsigned; leaving this state as char
# makes the decrement wrap to 255 and lets optimization remove the decoder
# initializer entirely.  Its exported symbol then aliases the next function,
# which crashes as soon as AVS+ AEC decoding starts.
cavs_start_addr=$(awk '$3 == "cavs_cabac_start_decoding" { print $1; exit }' <<< "$symbols")
cavs_stuffing_addr=$(awk '$3 == "cavs_biari_decode_stuffing_bit" { print $1; exit }' <<< "$symbols")
if [ -z "$cavs_start_addr" ] || [ -z "$cavs_stuffing_addr" ]; then
  echo "Missing AVS+ arithmetic decoder symbols" >&2
  exit 1
fi
if [ "$cavs_start_addr" = "$cavs_stuffing_addr" ]; then
  echo "AVS+ arithmetic decoder initializer was optimized to zero length" >&2
  exit 1
fi

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
