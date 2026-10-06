#!/bin/bash
# Also usable against libmpv.so extracted from the final HAP.
set -euo pipefail
LIBMPV=${1:?Usage: verify-ohos-http3-elf.sh /path/to/libmpv.so}
dynamic_section=$("${READELF:-llvm-readelf}" -d "$LIBMPV")
undefined_symbols=$("${NM:-llvm-nm}" -D --undefined-only "$LIBMPV")
if grep -Eq 'NEEDED.*lib(rcp|nghttp3)' <<< "$dynamic_section"; then
  echo 'HTTP/3 must not require system QUIC or shared nghttp3 libraries' >&2
  exit 1
fi
if grep -Eq '[[:space:]]nghttp3_[A-Za-z0-9_]+(@.*)?$' <<< "$undefined_symbols"; then
  echo 'nghttp3 must be statically resolved inside libmpv; unresolved imports:' >&2
  grep -E '[[:space:]]nghttp3_[A-Za-z0-9_]+(@.*)?$' <<< "$undefined_symbols" >&2
  exit 1
fi
if grep -Eq '[[:space:]]HMS_Rcp_Quic' <<< "$undefined_symbols"; then
  echo 'QUIC calls must use runtime symbol lookup' >&2
  exit 1
fi
echo 'HTTP/3 ELF static-link and optional QUIC checks passed'
