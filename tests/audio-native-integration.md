# OHAudio integration status

## Low-power playback

The power-saving patch maps MPV's existing `AO_INIT_MEDIA_ROLE_MUSIC`
(no video track) to OHAudio MUSIC only when AudioSuite is inactive.
Video and AudioSuite retain MOVIE. This is eligibility, not confirmation of
an active offload route: the OS and output device select the actual route.
The hardware-clock query interval is 250 ms in this music path, 20 ms otherwise.
PCM delivery retains the advanced callback and returns only actual copied
bytes; underrun does not fabricate silence. Seek/reset still flush the renderer
and reset stream/clock epochs. Hardware timestamps, not submitted bytes, remain
the playback clock source.

Device acceptance: test pure audio, video, effect on/off with AO reload,
speaker/wired/Bluetooth changes, pause, seek, EOS and screen-off background
playback. Verify actual offload with system audio diagnostics and measure
power: this patch alone does not prove power savings.

## Audio Vivid: native MPV path (device acceptance required)

The FFmpeg OHCodec bridge copies `OH_MD_KEY_AUDIO_VIVID_METADATA` into owned
AVFrame side data before releasing the codec buffer, with the original frame
size and native channel layout. Stream format callbacks are snapshotted per
queued frame instead of mutating the decoding context asynchronously.
The dedicated `ohaudio-vivid` AO receives whole frames (`write_frames`) and
uses `OH_AudioStreamBuilder_SetWriteDataWithMetadataCallback` with
`AUDIOSTREAM_ENCODING_TYPE_AUDIOVIVID`. Its bounded queue pairs PCM and metadata;
trimmed seek/EOS frames are dropped, never rendered with mismatched metadata.
Missing metadata, incompatible buffers and renderer initialization failures
raise an output error rather than falling back to ordinary PCM. Seek/reset
release the renderer before dropping frame references. Format/route changes
recreate the output; hardware timestamps drive delay estimates.

User-selected policy: Vivid takes priority. Native OHAudio speed (0.25–4x) is
applied only after both SetSpeed and GetSpeed confirm acceptance; rejection
restores/confirms the old rate without PCM fallback. Requests before the
renderer is ready retain 1x. Seek reapplies an accepted rate before restart.
The player keeps pitch unchanged, bypasses user audio filters, forced sample/channel conversion and the
AudioSuite AO. Ordinary PCM retains its existing AO. Gapless reuse cannot
carry an output across PCM/Vivid boundaries. The app rejects conflicting
effect requests based on the selected track, not the file's cached badge.
The native app speed setter returns the confirmed speed and a rejection reason;
UI/AVSession must use that result instead of displaying the requested value.

Build checks: the host packet test validates exact frame boundaries, metadata
size limits and non-mutation of destinations on invalid input. The final ELF
check requires the Vivid AO and SDK metadata/render callback references.
These checks do not certify spatial rendering, device support or power savings.

Device acceptance (user will perform): play a confirmed AV3A sample; check the
`Audio Vivid native metadata output` log; try effect and speed requests; pause,
seek and play to EOF; alternate ordinary/Vivid audio tracks and files; switch
speaker/headphones/Bluetooth. Check A/V sync and absence of stalls/leaks.
Native AVPlayer's separate PCM post-processing integration is not changed by
this MPV library patch; do not infer its native Vivid compatibility from this build.

References:
- https://raw.githubusercontent.com/openharmony/docs/master/zh-cn/application-dev/media/audio/power-saving-for-playback.md
- https://raw.githubusercontent.com/openharmony/docs/master/zh-cn/application-dev/media/audio/using-ohaudio-for-playback.md
- https://developer.huawei.com/consumer/cn/doc/HarmonyOS-Guides/audiovivid-audiodecoder
