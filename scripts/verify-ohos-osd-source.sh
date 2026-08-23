#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
SOURCE="$ROOT_DIR/libmpv/mpv/video/out/vo_ohcodec.c"

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
  '++global_osd.generation' \
  'generation == p->osd_surface_generation' \
  'p->osd_surface_generation = generation' \
  'p->osd_failed = true' \
  'destroy_osd_renderer(vo)' \
  'osd_render(' \
  'glDrawArrays(GL_TRIANGLE_STRIP' \
  'eglSwapBuffers('; do
  if ! grep -Fq "$contract" "$SOURCE"; then
    echo "Missing OSD generation/render lifecycle contract: $contract" >&2
    exit 1
  fi
done

echo "OHOS OSD source lifecycle contract verified"
