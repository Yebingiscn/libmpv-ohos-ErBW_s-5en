# libmpv-ohos-build

Build scripts of [libmpv](https://github.com/mpv-player/mpv) for ohos-arm64 (API 15+).

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

The build also adds:

- DVD navigation and CSS support (`libdvdnav`, `libdvdread`, `libdvdcss`)
- Blu-ray support (`libbluray`)
- archive/ISO9660 support (`libarchive`)
- an ISO9660 file lookup fallback when a DVD image has unreadable UDF metadata
- a fix for calculating the combined size of split VOB/AOB title files

The optical-media dependencies are linked statically into `libmpv.so`.
Enabling libdvdnav and libdvdcss changes the resulting combined work to GPL.

The output follows the current `mpv-arkts` native ABI. It does not restore the
old SweetVideo buffer-overlay renderer; the new OSD plane uses mpv's current
libass/OSD bitmap pipeline.

`libdvdcss` is included for CSS-encrypted DVDs. Blu-ray navigation is included,
but BD-J, `libaacs` and `libbdplus` are not; encrypted Blu-ray images therefore
still require an external decryption solution.

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
