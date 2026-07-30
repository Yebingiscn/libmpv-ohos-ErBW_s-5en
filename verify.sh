#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")"; pwd)

. "$ROOT_DIR/env.sh"

LIBMPV=$DEST/libmpv.so

if [ ! -f "$LIBMPV" ]; then
  echo "Missing build output: $LIBMPV" >&2
  exit 1
fi

features=$("$STRINGS" "$LIBMPV" | grep "^List of enabled features:")
configuration=$("$STRINGS" "$LIBMPV" | grep "^Configuration:")

for feature in dvdnav libarchive libbluray ohos; do
  if ! grep -Eq "(^| )$feature( |$)" <<< "$features"; then
    echo "Missing mpv feature: $feature" >&2
    exit 1
  fi
done

if ! grep -q -- "-Dgpl=true" <<< "$configuration"; then
  echo "mpv was not built with GPL support enabled" >&2
  exit 1
fi

symbols=$("$NM" -D --defined-only "$LIBMPV")
for symbol in mpv_create; do
  if ! grep -Eq "[[:space:]]$symbol$" <<< "$symbols"; then
    echo "Missing required dynamic symbol: $symbol" >&2
    exit 1
  fi
done

dynamic_section=$("$READELF" -d "$LIBMPV")
if grep -Eq "NEEDED.*lib(dvd|bluray|archive)" <<< "$dynamic_section"; then
  echo "Optical-media dependencies must be linked statically" >&2
  exit 1
fi

echo "$features"
echo "$configuration"
grep -E "NEEDED|SONAME" <<< "$dynamic_section"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$LIBMPV"
else
  shasum -a 256 "$LIBMPV"
fi
