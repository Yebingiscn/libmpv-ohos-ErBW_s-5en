# Startup and precise-seek decode demand

The decoder wrapper requests a preroll performance hint before the first
decoded picture and while a forward precise-seek target is pending. On reaching
that target it restores the normal policy. Both normal and repaired video use
the hint. It does not change playback speed, seek accuracy, timestamps, queue
sizes, or frame ownership.

OHCodec receives a frame-rate demand of at least 120 fps during preroll, or the
source rate times playback speed if that is larger. This is a best-effort
platform performance request, not a guaranteed decode throughput. Unsupported
keys or platform rejection retain the existing decode path. Actual startup
latency must be measured on device.

Adaptive frame retention is suppressed during preroll and during the existing
DTS-as-PTS repair candidate/active state. This prevents platform-level dropping
from invalidating the repair's frame count, including when the user selects a
higher playback speed. Normal video still uses the previous adaptive policy
after preroll. Setters deduplicate unchanged hints and platform updates remain
on the decoder thread.

`[Perf] codec-decode-hints` reports the requested frame rate, preroll/full-frame
flags and platform result for rate-only changes. Transitions from adaptive
retention also use the existing codec-policy diagnostics. Existing runtime
diagnostic controls apply.

The host test compiles the actual patched FFmpeg policy/setters against a
recording API. It verifies a 208-frame preroll, restoration, repeated-hint
deduplication, full-frame repair at 2x, normal adaptive playback, high source
rates, unsupported codecs/keys, and platform rejection.
