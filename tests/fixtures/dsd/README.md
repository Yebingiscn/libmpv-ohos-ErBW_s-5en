# DSD decoder fixtures

These are original, synthetic bit patterns, not music recordings. The generator
and generated data are provided under CC0-1.0.

`pattern.raw` contains 16,384 stereo samples, one chronological MSB-first DSD
byte per channel per sample (352,800 bytes/channel/second, DSD64).
The source DSF uses LSB-first bytes and 4096-byte channel blocks. It is generated
by a second-order one-bit modulator driven by synthetic 997/733 Hz sine waves.
See `generate.cjs` for the complete reproducible generator. It also creates a
4096-sample stereo 16-bit PCM WAV with deterministic integer values.
Reverse each DSD byte's bits for the interleaved `pattern.raw` reference. The DSF
header specifies two channels, 2,822,400 DSD bits/sec and 131,072 bits/channel.
The files were encoded with the official WavPack 5.9.0 Windows release:

```
wavpack -f pattern.dsf -o pattern-fast.wv
wavpack -hh pattern.dsf -o pattern-high.wv
wavpack pattern-pcm.wav -o pattern-pcm.wv
```

The source of the encoder is https://github.com/dbry/WavPack (BSD-3-Clause).
It is used only to produce regression fixtures, not added as a runtime dependency.
The two DSD files actually use encoding modes 1 and 3, respectively. Tests require
byte-for-byte equality with `pattern.raw`, reject corrupt CRCs, and verify that
ordinary PCM WavPack produces the same integer samples with `dsd_raw` off/on.
Mode 0 (uncompressed DSD) is constructed directly by the C test.

An optional external DST test used FFmpeg's FATE sample
https://fate-suite.ffmpeg.org/dst/dst-64fs44-2ch.dff,
SHA-256 `29d305bd19731ce079324ed14ac17254f254115d7787b5646a943e367b238ed0`.
That third-party sample is not redistributed here. Extract each `DSTF` payload
and prefix it with its uint32 little-endian size; pass this packet file as the
first argument to `tests/dsd-decoders.c`'s executable, before the four fixture
paths, to check compressed DST against the default decoder's PCM output.
