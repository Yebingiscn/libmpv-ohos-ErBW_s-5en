"""Source wiring guards; runtime behavior is exercised by the C host test."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
source = (root / "audio/out/ao_ohaudio.c").read_text(encoding="utf-8")
helper = (root / "audio/out/ohaudio_workgroup.h").read_text(encoding="utf-8")
callback = source.split("static int32_t audio_on_write_callback(", 1)[1].split("static int init(", 1)[0]
begin = callback.index("wg_started = ohaudio_wg_begin")
suite = callback.index("copied = render_audio_suite", begin)
pcm = callback.index("copied = ao_read_data_nonblocking", begin)
end = callback.index("if (wg_started) ohaudio_wg_end", begin)
assert begin < suite < end and begin < pcm < end, "PCM and Suite must share a balanced scheduling scope"
for forbidden in ("MP_INFO(", "MP_WARN(", "MP_ERR(", "dlopen(", "dlsym(", "ohaudio_wg_init("):
    assert forbidden not in callback, f"No callback logging/loading/creation: {forbidden}"
assert "CLOCK_REALTIME" in callback
assert "samples, output_rate, &wg_start, &wg_deadline" in callback
assert "(uint64_t)tid, wg_start, wg_deadline" in callback, "Use absolute system ms, not mpv presentation timestamp"
uninit = source.split("static void uninit(struct ao* ao)", 1)[1].split("static int render_audio_suite", 1)[0]
assert uninit.index("OH_AudioRenderer_Release") < uninit.index("ohaudio_wg_uninit")
assert "renderer_quiesced" in uninit
assert "default automatic policy covers PCM and AudioSuite" in source
assert "atomic_compare_exchange_strong_explicit" in helper
assert "\"libohaudio.so\"" in helper
for field, name in (("manager", "OH_AudioManager_GetAudioResourceManager"),
                    ("create", "OH_AudioResourceManager_CreateWorkgroup"),
                    ("release", "OH_AudioResourceManager_ReleaseWorkgroup"),
                    ("add", "OH_AudioWorkgroup_AddCurrentThread"),
                    ("remove", "OH_AudioWorkgroup_RemoveThread"),
                    ("start", "OH_AudioWorkgroup_Start"),
                    ("stop", "OH_AudioWorkgroup_Stop")):
    assert f"WG_LOAD({field}, {name});" in helper, f"Missing complete loader entry {name}"
if len(sys.argv) > 2 and sys.argv[2] == "--emit-callback":
    print("static int32_t audio_on_write_callback(" + callback)
else:
    print("OHAudio PCM/Suite workgroup wiring and callback safety guards passed")
