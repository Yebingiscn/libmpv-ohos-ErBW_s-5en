# HDR Vivid Vulkan transport prototype

This is the first-stage raw metadata transport and validation implementation.
It does not implement a Vivid tone mapper, generate replacement metadata, or
establish that the compositor consumed the supplied metadata correctly.
Eligible Vulkan output automatically attempts raw metadata transport. There is
no configuration switch or additional application UI. Confirm actual
`gpu-context=ohosvk`, `gpu-api=vulkan` and OHCodec hardware-decoded input in the
log. A rebuilt `libmpv.so` is required. For an A/B test, compare matching
libraries with and without this patch under otherwise identical conditions.
Do not infer library behavior from current source or an application's HDR badge.

## Transport contract

The prototype inspects the `OH_NativeBuffer` already acquired by the Vulkan
OHCodec mapper. It copies the platform's raw dynamic/static bytes into bounded
storage while that input buffer is valid. The C++ reader releases every
`OH_NativeBuffer_GetMetadataValue` allocation with `delete[]`, including partial
failure cases. It borrows the native buffer and does not release or retain the
owner's reference. An FFmpeg `AVDynamicHDRVivid` structure is not a native blob
and must not be copied directly into the platform metadata setter.

Source metadata carries the decoded-frame token, PTS and current epoch. Before
output acquire, passthrough checks the selected render frame and epoch, actual
high-bit-depth Vulkan output, and an unmodified source color pipeline. The
first frame may retain ordinary HDR output until the acquired swapchain format
is known. Unknown/missing/invalid metadata, old epochs, stale-token image reuse,
mixed frames and incompatible output processing must retain ordinary rendering.
Surface hard decoding, OpenGL, software decoding and VPE processing are not
established as Vivid passthrough by this prototype.

The source signal must remain in the correct PQ/HLG transfer, BT.2020 color
space and 10-bit-or-better output. Filters, LUT/ICC transforms, forced target
color/peak changes, inverse mapping and frame mixing can invalidate the raw
metadata relationship. Test the neutral pipeline before testing any of those
combinations. No CPU readback of full video frames is needed for metadata
inspection. Ineligible frames and unavailable metadata retain ordinary rendering.

Seek/reconfiguration invalidates the epoch so prior metadata cannot qualify for
a new timeline. If an old dynamic blob remains retained by the native window,
the fallback metadata type must prevent it from being interpreted as Vivid.
Do not fake an empty dynamic payload to clear it. A setter error is a fallback
condition, not evidence of device support. If restoring the basic metadata type
or negotiated color space also fails, prepare returns `-1`: the frame must not
be acquired/presented as though basic-HDR fallback were safe. A later successful
restore permits ordinary rendering again. Output audit runs only for a
successfully rendered and submitted Vivid frame, never a failed render/submit.

## Host validation

After applying the matching source baseline and all preceding patches:

```sh
MPV_SOURCE=/path/to/patched/mpv bash scripts/verify-ohos-vivid.sh
```

Use host `CC_FOR_BUILD`/`CXX_FOR_BUILD` (or `CC`/`CXX`) for these executable tests,
not the OHOS cross compiler. `PYTHON_FOR_BUILD` can select host Python 3.
The script checks frame/epoch/bounds/hash helpers and compiles the exact
production native reader against a test-only getter. It covers byte copying,
Vivid-vs-HLG/HDR10 identification, malformed sizes, optional static metadata,
null pointers, failed getters and allocation/deallocation balance. A third test
extracts the production output-transition functions into a temporary include
and exercises them with native setters/context stubs: ineligible frames preserve
negotiated color without writing the window; valid pass, token/epoch rejection, PQ static
metadata requirements, partial staging errors, unsafe restore failure, recovery
and seek invalidation. It never maintains a duplicate implementation. Source
checks verify Vulkan capture, seek reset, staging before output acquire,
withholding unsafe fallback, and successful-render/submit audit gating.
These tests do not exercise WSI, RenderService, panel tone mapping or device
performance; SDK object compilation and a complete library build are separate.

## Device acceptance

Use an ARM64 device and a known HEVC Main10 Vivid sample with independently
verified dynamic metadata. Include PQ and HLG variants, ordinary PQ/HDR10 and
ordinary HLG controls. Include a Vivid clip with changing scene metadata, not
only a scene whose metadata hash stays constant. Record the device model,
system/API version, output display and library hash extracted from the HAP.
Repeat the checks on phone, tablet, 2in1 and actual TV hardware before claiming
cross-device support; an x86_64 emulator is not TV validation.

1. Confirm `[Vivid] source` has valid bounded bytes for the Vivid
   sample and rejects the ordinary HLG/HDR10 controls. Record epoch, input PTS,
   native sequence, byte count and diagnostic hash. A native buffer sequence can
   be recycled, so it is not a unique frame ID across the entire session.
2. On eligible frames, correlate source records and
   `[Vivid] staged-before-acquire` records by epoch/token-associated PTS and hash.
   Verify presentation ordering with actual displayed frames. No metadata from
   a dropped frame, previous file or old seek epoch may qualify for another frame.
3. Inspect `[Vivid] last-flushed` diagnostics. WSI may report a previously
   presented buffer, including one-frame lag; compare against the appropriate
   recent source/staging history rather than assuming `hash == current` proves
   same-frame delivery. Missing or mismatched output metadata must be investigated.
4. A successful setter or matching last-flushed blob proves only that bytes were
   accepted/retained. Establish actual compositor/panel behavior using platform
   diagnostics and reference rendering or controlled visual/measurement tests.
   Do not use an HDR icon, overall brightness or one screenshot as the sole proof.
5. Repeat seek forward/back, high-speed playback and return to 1x, pause/repeated
   presentation, frame drop, rotation/resize, video changes and background/return.
   Include seek at a dynamic metadata scene cut. Test switching Vivid to ordinary
   HDR/SDR and back, without stale effects or flashes.
6. Confirm fallback for unavailable metadata, wrong frame/epoch, unsupported
   output precision and incompatible color processing. Compare matching builds
   with/without this patch and Surface playback. Then measure sustained CPU/GPU, power,
   dropped frames and memory at 1080p/4K and 30/60 fps under equal thermal state.

Record separately: host checks, SDK compilation, full build, metadata transport,
WSI frame association, compositor use, visual correctness and performance.
Successful transport alone is not a claim of complete HDR Vivid rendering.
