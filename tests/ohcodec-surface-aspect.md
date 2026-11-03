# Direct OHCodec/VPE aspect acceptance

The host regression extracts actual patched C functions and substitutes the
platform/device APIs. It checks preparation order, buffer geometry, allocation
failure, retirement, retained-frame ownership and cleanup. It does not emulate
the vendor VPE algorithm or prove physical display behavior.

Run `python3 scripts/verify-ohcodec-surface-aspect.py path/to/patched/mpv`.
`patch.sh` runs this after applying the full ordered patch series. The adapter
failure-injection suite also checks Stop without premature input destruction.

On an ARM64 device supporting VPE, test with detail enhancement enabled:

1. Use the reported 1920x1096 clip. Select 4:3, exit, reopen, then select the
   original aspect. Repeat starting with stretch-to-screen. The content must
   return to its natural aspect without additional letterbox bars.
2. Repeat 4:3/stretch/original changes in one session, including while paused,
   after rotation and following background/foreground transitions. Processing
   buffer geometry must retain the source DAR while the Surface changes size.
3. Open/loop/advance between landscape and portrait videos with different
   dimensions. Old VPE output must stop before a new producer is attached;
   retained old frames must be discarded rather than rendered or counted in
   the new generation. Return rapidly during preparation and playback.
4. Include non-square-pixel content, container rotation, and cropped content.
   Check picture aspect and the independently rendered subtitles/OSD.
5. Repeat with super resolution disabled, with SDR-to-HDR enabled, and in GPU
   buffer/software modes. These paths keep their existing processing behavior.
6. Unsupported VPE initialization must retain ordinary Surface hardware output.
   A failed Stop must prevent a second producer claiming the same Surface.

The diagnostic reports source dimensions/SAR and selected VPE output dimensions
before decoder open. `configured output=...` is a requested output configuration,
not a measurement of physical buffers. Compare screenshots/content bounds as
well as Surface lifecycle logs; successful API calls alone are insufficient.
