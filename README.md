# libmpv-ohos-build

Build scripts of [libmpv](https://github.com/mpv-player/mpv) for OpenHarmony ARM64 and x86_64.

CI builds ARM64 only. The `artifact` download contains `libmpv_aarch64.zip`
with its `libmpv.so`; CI does not build or package x86_64.

For a local Linux/macOS build, run `MPV_BUILD_ARCH=arm64 ./bundle.sh` (default) or
`MPV_BUILD_ARCH=x86_64 ./bundle.sh`. Do not use `TARGET_ARCH`: GNU Make treats
that variable as compiler flags. Use a **separate fresh checkout per architecture**:
dependency source directories contain in-tree build caches and cannot be shared
between architectures. Outputs go to `libmpv/<architecture>-build/`.
Install NASM for x86_64 assembly. ARM here means ARM64, not 32-bit ARM.
The emulator also needs x86_64 app-side native libraries; hardware decoding,
HDR and VPE availability depend on its system image and GPU capabilities.
The AudioSuite integration requires API 23+, with HOA rendering requiring API 26.
Linux CI uses the pinned HarmonyOS 7.0 Release public NDK `26.0.0.38`
(`20260829` archive, verified by SHA-256). Its AudioSuite base and engine headers
are identical to the local DevEco `26.0.0.105` SDK headers.
The SDK preflight checks version, API 26 AVCodec declarations, and the HOA node;
the final binary check also requires the `ohaudiosuite-hoa` feature.

Scripts are compatible with macOS, Linux and WSL, Windows is not supported.

This tree is based on the `20260715` mpv-arkts binary baseline and pins
`ErBWs/mpv` to commit `6edeee00a07b9b76f197aa71eee3d029fb090de4`.

## OHCodec Surface output

The `patches/mpv/support-ohcodec-surface-osd.patch` patch adds the
`ohcodec-osd` video output:

- decoded OHCodec frames are presented directly to the Surface passed through
  `--wid`;
- mpv/libass subtitles and OSD are rendered into a second transparent BGRA
  Surface;
- the embedding library supplies or replaces that second Surface with
  `ohos_osd_set_global_surface(surface_id, width, height)`;
- the OSD Surface may be supplied before or after `mpv_initialize()`.

The two ArkUI surfaces must have the same on-screen bounds, with the transparent
OSD XComponent above the video XComponent. The application remains responsible
for making the upper XComponent transparent.

The existing `gpu-next` OHCodec OpenGL/Vulkan path remains available as buffer
mode. `ohcodec-osd` is the direct Surface mode: it avoids a video-frame copy,
but mpv GPU shaders such as Anime4K do not run on the direct video plane.
Subtitles and normal mpv OSD remain available through the separate OSD plane.

The OHCodec buffer path treats NativeImage as a single mutable external image.
It therefore disables temporal frame mixing and requests one decoder frame at
a time. This keeps the zero-copy path short and prevents playback-speed changes
from exhausting the decoder output pool. Spatial GPU shaders remain available;
software-decoded video is unaffected and may still use temporal interpolation.

## System video super resolution

SDR-to-HDR takes priority over super resolution. When `ohos-sdr-to-hdr` is
`pq` or `hlg`, VPE is bypassed even if `ohos-super-resolution=yes`. Changing
SDR-to-HDR recreates the video output so an existing VPE chain is removed;
turning it off allows the requested super resolution setting to apply again.

`--ohos-super-resolution=yes` enables VPE detail enhancement at fixed HIGH
quality. The default is `no`. There are no selectable quality levels. The same
option covers direct OHCodec Surface output, GPU hardware decoding, and software
decoding, including both OpenGL and Vulkan GPU contexts. GPU processing enhances
the rendered output; direct decoding feeds the VPE input Surface before display.
Direct-mode subtitles remain on their separate OSD Surface.

Changing the option recreates the video output and decoder through mpv's normal
VO update path, restoring the current playback position. The VPE input remains
alive until its producer is released. Unsupported initialization uses normal
output; asynchronous processing errors switch the live option off and rebuild
normal output. Logs use the `[SuperResolution]` prefix. Device support and visual
quality require on-device testing; VPE is not guaranteed to match AVPlayer's SR.

VPE output callbacks enqueue buffer tokens for an application-owned worker;
they never call back into VPE rendering. Once frame submission begins, missing
the first output for three seconds requests normal output and wakes the VO.
This guard does not count file loading time or require performance statistics.
Successful buffer submission is logged once; it does not prove physical display.

`scripts/verify-vpe-adapter.sh` exercises initialization failures, fixed HIGH
quality, output callbacks, resize, fallback, and failed vendor destruction.
`verify.sh` requires the final binary to include the option and VPE dependency.

## Runtime performance diagnostics

The performance patches add debug-level `[Perf]` records for packet/frame calls,
OHCodec queue waits and policy changes, direct Surface submission and OSD work,
and OHAudio hardware-clock queries/underflows. Request `debug` through
`mpv_request_log_messages()` to enable them and `info` to disable them without
rebuilding or restarting the core. FFmpeg instrumentation follows the decoder's
runtime log level via `avcodec_ohcodec_set_diagnostics()`; no audio callback logs
or per-callback timing counters are introduced. `verify.sh` checks the markers
in the built library.

The SweetVideo bridge adds 250ms property summaries and a hidden `mpvDiagnostics`
boolean Want parameter, with separate bounded frame-log and ordinary-log budgets.
New instrumentation requires one updated core build; subsequent toggles are
runtime-only. API call timings measure host-side work and waiting, not physical
screen presentation or hardware execution time. Missing/limited log records
must not be interpreted as dropped video frames.

## Automatic OHCodec playback policy

Both OHCodec output paths automatically pass the source frame rate to the
decoder and request decoder-side variable refresh rate support. Playback-speed
changes are forwarded internally to OHCodec: on systems exposing the API 26
smart-fluency keys, speeds above 1x use adaptive frame retention and returning
to 1x restores full retention.

Policy results are emitted automatically with the `[OHCodecPolicy]` prefix at
warning level so hosts using the default mpv warning log level can diagnose the
feature without a user-facing setting. A successful decoder request reports
`vrr=requested ... configure=ok` for either `mode=surface` or `mode=buffer`;
smart fluency reports `smart-fluency=adaptive ... set-parameter=ok` and reports
`smart-fluency=full` when playback returns to 1x.

The API 26 SDK declarations and official `OH_FrameRetentionMode` values are
used at compile time, while optional metadata-key symbols are resolved with
`dlsym` and are never hard-linked. Older HarmonyOS releases therefore keep the
normal playback path without requiring an application target-SDK change or
user-facing settings.

The build also adds:

- AVS+ / AVS1-P16 software decoding through FFmpeg's `cavs` decoder
- DVD navigation and CSS support (`libdvdnav`, `libdvdread`, `libdvdcss`)
- Blu-ray support (`libbluray`)
- archive/ISO9660 support (`libarchive`)
- an ISO9660 file lookup fallback when a DVD image has unreadable UDF metadata
- a fix for calculating the combined size of split VOB/AOB title files

The optical-media dependencies are linked statically into `libmpv.so`.
Enabling libdvdnav and libdvdcss changes the resulting combined work to GPL.

AVS+ support is based on the public
[`ffmpeg_cavs_dra`](https://github.com/maliwen2015/ffmpeg_cavs_dra)
implementation pinned in `download/deps-version.sh`. The build imports only
its `libcavs` video decoder files into FFmpeg 8.1.2; the DRA audio decoder is
not included. mpv selects the resulting `cavs` decoder automatically for CAVS
streams, including AVS1-P16 broadcast TS files, with no player-side option.

The output follows the current `mpv-arkts` native ABI. It does not restore the
old SweetVideo buffer-overlay renderer; the new OSD plane uses mpv's current
libass/OSD bitmap pipeline.

`libdvdcss` is included for CSS-encrypted DVDs. Blu-ray navigation is included,
but BD-J, `libaacs` and `libbdplus` are not; encrypted Blu-ray images therefore
still require an external decryption solution.

OHAudio translates the hardware CLOCK_MONOTONIC timestamp into mpv's timebase
before calculating queued audio. A coherent atomic snapshot lets the audio
callback read clock updates without waiting for the background query thread.
Audio callbacks have no performance timing counters or periodic statistics;
clock availability and AudioSuite errors are reported on state changes by the
background thread. The patch stage tests timestamp conversion, stale samples,
stream epochs, and concurrent snapshot publication on the build host.

OHAudio PCM playback tracks each queued audio frame's actual speed and timestamp.
Both the playback clock and video scheduling use this timeline when a speed
change leaves old- and new-speed audio in the output queue. This avoids clock
jumps on acceleration and restoration without flushing buffered audio. Missing
timestamps and unavailable history retain mpv's existing timing fallback. The
patch stage runs regression tests for both speed-change directions, rapid
switching, timestamp gaps, and bounded history.

Runtime debug diagnostics include `[Perf] audio-timeline` every 250 ms. OHCodec
queue/policy diagnostics use FFmpeg's verbose level, which maps to mpv debug,
so the existing hidden diagnostics switch also enables these records.

Decoder throughput now stays high until queued faster audio has drained when
speed is restored. Video sync projections and frame durations also follow the
queued timeline, avoiding false frame-drop requests during transitions.
OHCodec prepares the built-in OSD font provider before presenting video; the
first font scan adds startup preparation time instead of interrupting playback
and starving audio. Debug records `decode-demand`, `osd-prewarm`, `font-setup`,
and slow `osd-prepare` calls identify the remaining transition/startup costs.

## Build Dependencies

- git
- make
- python3
- pkg-config
- gperf
- meson

ohos sdk is automatically downloaded on Linux / WSL, but you need to manually download DevEco Studio on your mac.

## Build

```shell
chmod +x *.sh */*.sh
./bundle.sh
```

`bundle.sh` validates the enabled mpv features, public libmpv API,
static optical-media linkage, dynamic dependencies, and SHA-256 before creating
`libmpv/arm64-build/libmpv_aarch64.zip`.
