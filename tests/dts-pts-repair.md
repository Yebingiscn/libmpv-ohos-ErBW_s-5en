# Incorrectly muxed CFR timestamps

`video-repair-dts-pts=yes` (default) enables a narrow automatic fallback in the
decoder wrapper. `no` disables it without rebuilding the library. It does not
require a frontend control.

The detector checks original packet timestamps before mpv synthesizes missing
DTS: at least eight consecutive packets must have PTS equal to DTS, duration
equal to the declared frame interval, and a matching constant DTS cadence.
There must also be at least two decoded PTS regressions in the first 64 frames
after a decoder reset. Monotonic low-delay video is not retimed. Missing
timestamps, variable cadence, or real PTS/DTS offsets disqualify the epoch.

Once detected, frames receive `first decoded PTS + frame index / fps` before
caption timestamps and seek clipping. The initial detection may cause one
backwards correction to the original time origin; it does not carry a permanent
offset from the bad initial frames. No extra frame buffers or startup waits are
introduced. Playback speed does not enter this media-time calculation.

Decoder frame dropping is disabled during the initial candidate window and
while repair is active, since counting requires every decoded frame. Later
presentation dropping is unchanged. Seeking preserves a confirmed diagnosis
and establishes a new origin from decoded preroll, rather than the seek target.
Decoder reinitialization clears the diagnosis. Packet cadence changes cancel
repair. Reverse playback and explicit `correct-pts=no` bypass this fallback.

Tests cover the reported HEVC sequence, seek reset, unaffected normal video,
valid reordered timestamps, VFR, discontinuities, missing timestamps, and an
isolated glitch. An optional text file of raw decoded PTS can be passed to the
test executable to replay a longer sample. Hardware playback still requires
device verification after the build.
