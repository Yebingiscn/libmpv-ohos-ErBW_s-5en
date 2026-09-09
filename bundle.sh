#!/bin/bash

set -eu

./download.sh
./patch.sh
./build.sh
./verify.sh

. ./env.sh
cd "$DEST"
zip "libmpv_$ARCHIVE_ARCH.zip" libmpv.so
