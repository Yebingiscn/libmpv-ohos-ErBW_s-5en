# Experimental E-AC3 integration

This backend is an experimental implementation, not a verified device support
claim. No phone, tablet, 2in1, TV or receiver has been validated for it yet.

## Contract requiring device/vendor confirmation

The public SDK provides E_AC3 and a bitstream capability query, but the retrieved
public sources do not establish the compressed callback framing or timestamp
unit on Huawei devices. This implementation assumes native E_AC3 accepts raw
codec bytes (without IEC 61937 preamble/padding), permits one complete access
unit per advanced callback, and reports decoded 48 kHz sample positions. It
configures a stereo transport descriptor; multichannel contents remain encoded.
These assumptions must be confirmed before treating the feature as stable.
The application labels the setting experimental and leaves it disabled.

## Scope and fallback

- Opt in using `ohaudio-eac3-enabled=yes` and `audio-spdif=eac3` together.
- Only complete 48 kHz E-AC3 access units are accepted. AC-3, DTS, DTS-HD,
  TrueHD, other rates and incomplete bursts are not supported by this backend.
- FFmpeg's SPDIF muxer packs complete bursts; the AO validates and unwraps them.
- The current route must explicitly report BITSTREAM support, not PCM direct.
- Missing optional symbols and initialization failures use mpv's PCM fallback.
- Route changes, malformed/trimmed bursts, undersized callbacks, renderer errors
  and a stalled clock request PCM decoder fallback on the player thread.
- No compressed data reaches ordinary OHAudio or Audio Suite.
- The app disables passthrough for non-1x speed and enabled audio effects.
- An older bundled core rejects the backend opt-in. The app leaves audio-spdif
  empty; a settings-only HAP therefore does not enable actual passthrough.

## Required device checks

1. Confirm raw E-AC3 framing, callback size/return units, 48 kHz timestamp units
   and stereo transport descriptor against the target OS/vendor implementation.
2. Connect a capable receiver. Confirm its E-AC3 indication, audible channels,
   lip sync and output stability. Renderer creation alone is not proof.
3. Test 1/2/3/6-block frames, dependent substreams, seek, pause/resume, EOF and
   consecutive files. Unsupported frames must recover as PCM, without retry loops.
4. Disconnect/change route while running and while paused; verify PCM fallback.
5. Enable/disable audio effects, change speed and audio tracks; check restoration
   of PCM processing and preferences. Test unsupported receivers and speaker output.
6. Check final HAP libraries' ABI, dependencies and unresolved symbols. Build the
   patched core on Linux/macOS/WSL before replacing the application's library.

Packet tests use synthetic transport frames; they do not prove codec decoding,
audibility, renderer acceptance, device compatibility or clock correctness.
