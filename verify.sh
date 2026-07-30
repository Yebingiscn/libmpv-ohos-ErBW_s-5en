#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd $(dirname "$0"); pwd)

. $ROOT_DIR/env.sh

LIBMPV=$DEST/libmpv.so

if [ ! -f "$LIBMPV" ]; then
  echo "Missing build output: $LIBMPV" >&2
  exit 1
fi

features=$("$STRINGS" "$LIBMPV" | grep "^List of enabled features:")
configuration=$("$STRINGS" "$LIBMPV" | grep "^Configuration:")

for feature in dvdnav libarchive libbluray ohos; do
  if ! printf '%s\n' "$features" | grep -Eq "(^| )$feature( |$)"; then
    echo "Missing mpv feature: $feature" >&2
    exit 1
  fi
done

if ! printf '%s\n' "$configuration" | grep -q -- "-Dgpl=true"; then
  echo "mpv was not built with GPL support enabled" >&2
  exit 1
fi

symbols=$("$NM" -D --defined-only "$LIBMPV")
for symbol in mpv_create; do
  if ! printf '%s\n' "$symbols" | grep -Eq "[[:space:]]$symbol$"; then
    echo "Missing required dynamic symbol: $symbol" >&2
    exit 1
  fi
done

if "$READELF" -d "$LIBMPV" | grep -Eq "NEEDED.*lib(dvd|bluray|archive)"; then
  echo "Optical-media dependencies must be linked statically" >&2
  exit 1
fi

echo "$features"
echo "$configuration"
"$READELF" -d "$LIBMPV" | grep -E "NEEDED|SONAME"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$LIBMPV"
else
  shasum -a 256 "$LIBMPV"
fi
