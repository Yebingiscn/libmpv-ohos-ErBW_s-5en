#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
SOURCE="$ROOT_DIR/libmpv/mpv/video/out/vo_ohcodec.c"
FFMPEG_DEC="$ROOT_DIR/libmpv/ffmpeg/libavcodec/ohdec.c"
FFMPEG_HWCTX="$ROOT_DIR/libmpv/ffmpeg/libavutil/hwcontext_oh.h"

if [ ! -f "$SOURCE" ]; then
  echo "Missing patched OHCodec VO source: $SOURCE" >&2
  exit 1
fi

# The OSD producer must have exactly one terminal operation: eglSwapBuffers.
# Request/Map/Flush/Abort belonged to the removed CPU full-frame path and can
# recreate the CancelBuffer use-after-free when ArkUI destroys a Surface.
for symbol in OH_NativeWindow_NativeWindowRequestBuffer \
              OH_NativeWindow_NativeWindowMap \
              OH_NativeWindow_NativeWindowFlushBuffer \
              OH_NativeWindow_NativeWindowAbortBuffer; do
  if grep -Fq "$symbol" "$SOURCE"; then
    echo "Legacy OSD buffer lifecycle call remains: $symbol" >&2
    exit 1
  fi
done

for contract in \
  'atomic_fetch_add_explicit(' \
  'memory_order_acq_rel' \
  'atomic_store_explicit(' \
  'memory_order_release' \
  'generation == p->osd_surface_generation' \
  'p->osd_surface_generation = generation' \
  'p->osd_failed = true' \
  'destroy_osd_renderer(vo)' \
  'osd_render(' \
  'glTexSubImage2D(' \
  'hwctx->direct_surface = 1' \
  'glDrawArrays(GL_TRIANGLE_STRIP' \
  'eglSwapBuffers('; do
  if ! grep -Fq "$contract" "$SOURCE"; then
    echo "Missing OSD generation/render lifecycle contract: $contract" >&2
    exit 1
  fi
done

if [ "$(grep -Fc 'eglMakeCurrent(' "$SOURCE")" -ne 2 ] ||
   [ "$(grep -Fc 'eglQuerySurface(' "$SOURCE")" -ne 2 ]; then
  echo "Per-frame EGL context/surface query returned to the OSD hot path" >&2
  exit 1
fi

for contract in \
  'OH_DOVI_PROBE_PACKETS' \
  's->direct_surface_output' \
  's->dovi_metadata_enabled'; do
  if ! grep -Fq "$contract" "$FFMPEG_DEC"; then
    echo "Missing OHCodec DOVI parsing gate: $contract" >&2
    exit 1
  fi
done
if ! grep -Fq 'int direct_surface;' "$FFMPEG_HWCTX"; then
  echo "Missing OHCodec direct-Surface device marker" >&2
  exit 1
fi

echo "OHOS OSD source lifecycle contract verified"
