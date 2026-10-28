#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh

# Explicit ThinLTO exception: Rust stays on its own release/staticlib path.
# Do not pass C/C++ -flto=thin or linker-plugin-lto here without matching the
# Rust compiler's LLVM to the OHOS SDK. Host proc-macros must remain host code.

pushd $ROOT_DIR/libmpv/dovi_tools/dolby_vision

if [ "$1" == "build" ]; then
	echo -e "\nBuilding dovi tools..."
elif [ "$1" == "clean" ]; then
	cargo clean
	exit 0
else
	exit 1
fi

cargo cinstall \
  --release \
  --target="$RUST_TARGET" \
  --library-type=staticlib \
  --prefix=$DEST \
  --libdir=lib

popd
