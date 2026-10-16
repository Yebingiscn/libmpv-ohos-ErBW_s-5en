# GLES software upload acceptance

The `zzz-gles-software-upload-pool.patch` targets GLES 3 software-frame uploads
with asynchronous source-lifetime callbacks. It retains cached CPU decoder
reference frames and stages unchanged YUV bytes in pixel-unpack buffers.
It does not implement decoder zero-copy or lower image quality.

Each GL GPU/context has at most six lazily allocated slots and 64 MiB total
staging capacity. Planes below 32 KiB or above 16 MiB use the original client
pointer upload. Busy slots are checked with zero timeout; no fence wait is
introduced. Mapping or synchronization errors disable staging for that context.
The source callback runs exactly once on successful upload: after staging has
copied the source, or after the original asynchronous transfer. Failed uploads
leave source cleanup to the caller. Context destruction drains GPU work before
deleting the pool. Vulkan and hardware external textures do not use this pool.

Before claiming a speedup, compare otherwise identical libraries with/without
this patch on an ARM64 device, using the same GLES backend, video, settings,
screen brightness and temperature. After warmup, measure sustained CPU, decoder
and VO thread time, upload time, dropped frames, memory, power and thermal state.
Test VP9 8bit and 10bit, 1080p/4K, 30/60 fps; compare decoded output and rendered
images. Include repeated seeks, pause/resume, speed changes, media changes,
rotation and background/foreground transitions. Check software fallback, Vulkan
and Surface hard decoding for regressions.

The one-time `GLES software frame upload: bounded PBO staging (64 MiB)` message
confirms staging was successfully submitted; it does not prove higher speed.
`GLES software upload staging disabled; using client memory` reports a prepare
failure and fallback. GPU submission/sync failures can reject the current frame;
subsequent uploads use client memory. No per-frame logging is added.

`scripts/verify-gles-upload.sh` tests the production pool against a mock driver,
including busy slots, reuse, byte contents, size/budget limits, growing frames,
map/unmap/fence failures and cleanup. It is not a GLES/device test.
