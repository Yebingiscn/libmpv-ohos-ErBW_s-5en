#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
SOURCE="$ROOT_DIR/libmpv/mpv/video/out/vo_ohcodec.c"
GPU_SOURCE="$ROOT_DIR/libmpv/mpv/video/out/vo_gpu_next.c"
OHOS_COMMON="$ROOT_DIR/libmpv/mpv/video/out/ohos_common.c"
OHOS_GL="$ROOT_DIR/libmpv/mpv/video/out/opengl/context_ohos.c"
OHOS_VK="$ROOT_DIR/libmpv/mpv/video/out/vulkan/context_ohos.c"
FFMPEG_DEC="$ROOT_DIR/libmpv/ffmpeg/libavcodec/ohdec.c"
FFMPEG_HWCTX="$ROOT_DIR/libmpv/ffmpeg/libavutil/hwcontext_oh.h"

if [ ! -f "$SOURCE" ]; then
  echo "Missing patched OHCodec VO source: $SOURCE" >&2
  exit 1
fi

for source in "$GPU_SOURCE" "$OHOS_COMMON" "$OHOS_GL" "$OHOS_VK"; do
  if [ ! -f "$source" ]; then
    echo "Missing patched OHOS source: $source" >&2
    exit 1
  fi
done

# ArkUI's free-running frame callback is not presentation feedback. Keep that
# integration fully removed; EGL/Vulkan presentation remains compositor-paced
# by their native swapchain semantics.
for symbol in mpv_ohos_report_vsync mpv_ohos_reset_vsync \
              vo_ohos_vsync_attach vo_ohos_vsync_detach \
              vo_ohos_get_vsync vo_ohos_get_display_fps \
              eglSwapInterval; do
  if grep -Fq "$symbol" "$SOURCE" "$OHOS_COMMON" "$OHOS_GL" "$OHOS_VK"; then
    echo "Removed OHOS VSYNC integration remains: $symbol" >&2
    exit 1
  fi
done

# OHCodec no longer forces vo_gpu_next to request a single decoded frame. The
# ordinary mpv/libplacebo request depth is retained, while temporal mixing is
# still gated because NativeImage exposes one mutable external image.
for contract in \
  'bool mutable_image_hwdec;' \
  'int req_frames = 2;' \
  'opts->interpolation && !p->mutable_image_hwdec'; do
  if ! grep -Fq "$contract" "$GPU_SOURCE"; then
    echo "Missing OHCodec queue/interpolation contract: $contract" >&2
    exit 1
  fi
done
if grep -Fq 'decoder queue depth=1' "$GPU_SOURCE"; then
  echo "OHCodec single-frame queue limit returned" >&2
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
