#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/mbedtls

if [ "$1" == "build" ]; then
	echo -e "\nBuilding mbedtls..."
elif [ "$1" == "clean" ]; then
	make clean
	rm -f .build/.sweetvideo-lto-mode .build/.sweetvideo-make-lto-inputs
	exit 0
else
	exit 1
fi

prepare_lto_build mbedtls "$PWD/.build" "$PWD"
python3 -m venv .venv
source .venv/bin/activate
pip install -r scripts/basic.requirements.txt

# Keep the upstream -O2 default (env CFLAGS otherwise hides its ?= value).
# Pass the same settings to install, whose no_test dependency can build files.
mbedtls_make_args=("CC=$CC" "AR=$AR" AR_DASH= "RANLIB=$RANLIB"
  "RL=$RANLIB" "CFLAGS=-O2 $MAKE_LTO_CFLAGS" "LDFLAGS=$MAKE_LTO_LDFLAGS")
record_make_lto_inputs "$PWD/.build" "$PWD/library" "${mbedtls_make_args[@]}"
make "${mbedtls_make_args[@]}" -j$CORES no_test
make "${mbedtls_make_args[@]}" DESTDIR=$DEST install

popd
