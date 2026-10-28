#!/bin/bash

set -eu

ROOT_DIR=$(cd $(dirname "$0")/..; pwd)

. $ROOT_DIR/env.sh
. "$ROOT_DIR/scripts/build-lto.sh"

pushd $ROOT_DIR/libmpv/lua

if [ "$1" == "build" ]; then
	echo -e "\nBuilding lua..."
elif [ "$1" == "clean" ]; then
	make clean
	rm -f .build/.sweetvideo-lto-mode .build/.sweetvideo-make-lto-inputs
	exit 0
else
	exit 1
fi

prepare_lto_build lua "$PWD/.build" "$PWD/src"
# Preserve upstream -O2/warnings/compatibility flags via MYCFLAGS, not CFLAGS.
# This is the Linux library part of upstream's linux target; no interpreter or
# luac is needed for embedding, so no target executable is built or run.
lua_make_args=("CC=$CC" "AR=$AR rcu" "RANLIB=$RANLIB"
  "MYCFLAGS=$MAKE_LTO_CFLAGS" "MYLDFLAGS=$MAKE_LTO_LDFLAGS"
  SYSCFLAGS=-DLUA_USE_LINUX LUA_T= LUAC_T=)
record_make_lto_inputs "$PWD/.build" "$PWD/src" "${lua_make_args[@]}"
make -C src "${lua_make_args[@]}" -j$CORES liblua.a

# Cheat make on macOS to copy makefile itself to bin instead of install lua & luac
make INSTALL_TOP=$DEST TO_BIN="Makefile" install

make INSTALL_TOP=$DEST pc > $DEST/lib/pkgconfig/lua.pc
cat >> $DEST/lib/pkgconfig/lua.pc <<'EOF'

Name: Lua
Description:
Version: ${version}
Libs: -L${libdir} -llua
Cflags: -I${includedir}
EOF

popd
