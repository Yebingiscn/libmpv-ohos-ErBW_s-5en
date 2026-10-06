# Optional QUIC / HTTP/3 acceptance

The host regression uses real nghttp3 framing/QPACK and a mocked RCP transport.
It is not a real QUIC/TLS handshake, a device test, or a weak-network benchmark.
Run `bash scripts/test-ohos-http3.sh` after download and patching on Linux.

## Device coverage

Huawei documents RCP QUIC API 26 for phone, tablet, PC/2in1 and TV:
https://developer.huawei.com/consumer/en/doc/harmonyos-references/_rcp___quic_stream_data
API coverage does not establish library availability in each device namespace.
Test ARM64 devices separately. Older or unsupported systems must retain TCP/TLS
without loader failures. The public NDK build uses an ABI subset checked against
the HMS 26.0.0.105 header, not a packaged/copied system library.

## Required tests

- Plain HTTP IPTV, UDP/RTP, RTSP, local files and existing decoding/VO modes
  must remain unchanged. QUIC is only eligible for HTTPS GET.
- An initial TCP response advertising `Alt-Svc: h3=":443"` makes subsequent
  same-authority connections eligible. No URL rewriting or user setting.
  HLS playlists, segments and keys each follow their actual HTTPS origins.
- Only the original hostname and port are supported in this first experiment.
  Alternative hosts/ports, explicit proxies and custom client certificates use
  the existing transport. TLS verification is never relaxed by the bridge;
  verified requests need the same explicit CA file as the Mbed TLS backend.
- For an eligible source expect `[OHHTTP3] HTTP/3 response received; QUIC active`.
  This is emitted once per cached origin after parsing a real H3 response, not
  just after resolving symbols or asking for QUIC.
- Block UDP or use an HTTP/3-unavailable endpoint after advertisement. Expect
  normal TCP/TLS playback; failed H3 attempts back off for five minutes. Check
  Range retries at nonzero offsets and that cancel doesn't start a fallback.
- Check 206, 301/302 redirects, 401/403/404, signed headers, Referer, cookies,
  gzip, embedded zero bytes, live EOF/reconnect, and HLS key/segment retrieval.
- HLS keep-alive may reuse up to eight requests per H3 connection. Then rotate
  the connection to bound nghttp3 send state (RCP has no public ACK callback).
- Continuous TS/FLV: run at least one hour, check memory remains stable, and
  compare start time/stalls against TCP. The app receive rings are bounded;
  system QUIC buffers and actual flow-control behavior need device measurement.
- Repeated open/cancel/release and concurrent players: check no late callbacks
  access a released FFmpeg context. If the runtime doesn't confirm connection
  closure, callback state is retained and H3 disabled until process exit. This
  safety fallback is logged and must be investigated, not called a success.

For final packaged binaries check ARM64 ELF architecture, NEEDED and undefined
symbols. Neither librcp_quic.so/librcp_quic_c.so nor libnghttp3.so may be required;
there must be no direct `HMS_Rcp_Quic*` or unresolved `nghttp3_*` imports.
Run `scripts/verify-ohos-http3-elf.sh` against the extracted libmpv.so.
The installed static libavformat pkg-config flags must include `-lnghttp3`;
the source regression exercises enabled/disabled dependency flattening using
FFmpeg's actual helpers. `tests/http3-link-probe.c` references all ten nghttp3
APIs from the failing artifact for an SDK ARM64 static-link check. These checks
are separate from a full libmpv build and real-device playback. Device improvement remains
unverified until real sources and controlled network conditions are tested.

The Linux host protocol script also builds linked/unlinked ELF probes with real
nghttp3 and checks acceptance/rejection using the production ELF guard. Its
nghttp3 build uses PIC and independent temporary caches, not target outputs.
