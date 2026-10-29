#!/bin/bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
SDK_NATIVE=${1:?Usage: cmake-lto-abi.sh NATIVE_SDK_DIR TARGET_TRIPLE}
TRIPLE=${2:?Missing target triple}
case "$TRIPLE" in aarch64-linux-ohos|x86_64-linux-ohos) ;; *) exit 1 ;; esac
LLVM="$SDK_NATIVE/llvm/bin"
EXE=
if [ -f "$LLVM/clang.exe" ]; then EXE=.exe; fi
CMAKE="$SDK_NATIVE/build-tools/cmake/bin/cmake$EXE"
NINJA="$SDK_NATIVE/build-tools/cmake/bin/ninja$EXE"
MPV_LTO_JOBS=4
CFLAGS='-fPIC -D__MUSL__=1'
CXXFLAGS=$CFLAGS
AR="$LLVM/llvm-ar$EXE"
RANLIB="$LLVM/llvm-ranlib$EXE"
NM="$LLVM/llvm-nm$EXE"
. "$ROOT_DIR/scripts/build-lto.sh"

mkdir -p "$ROOT_DIR/libmpv"
CHECK_PARENT=$(cd "${TMPDIR:-$ROOT_DIR/libmpv}"; pwd -P)
CHECK_DIR=$(mktemp -d "$CHECK_PARENT/cmake-lto-abi.XXXXXX")
cleanup() {
  # Only remove the exact fresh test directory under its resolved parent.
  case "$CHECK_DIR" in "$CHECK_PARENT"/cmake-lto-abi.*) rm -rf -- "$CHECK_DIR" ;; *) return 1 ;; esac
}
trap cleanup EXIT
common=(-S "$ROOT_DIR/tests/thinlto/cmake-abi" -G Ninja
  "-DCMAKE_MAKE_PROGRAM=$NINJA" -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_SYSTEM_NAME=Linux "-DCMAKE_SYSTEM_PROCESSOR=${TRIPLE%%-*}"
  "-DCMAKE_C_COMPILER=$LLVM/clang$EXE" "-DCMAKE_CXX_COMPILER=$LLVM/clang++$EXE"
  "-DCMAKE_C_COMPILER_TARGET=$TRIPLE" "-DCMAKE_CXX_COMPILER_TARGET=$TRIPLE"
  "-DCMAKE_SYSROOT=$SDK_NATIVE/sysroot" "${CMAKE_LTO_ARGS[@]}")

# This is the old nghttp3 failure: CMake reads ABI markers from the generated
# artifact, but a static archive of ThinLTO IR does not expose native markers.
"$CMAKE" "${common[@]}" -B "$CHECK_DIR/static" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DREQUIRE_ABI=OFF \
  > "$CHECK_DIR/static.log" 2>&1 || { cat "$CHECK_DIR/static.log"; exit 1; }
if grep -Fx 'C_BYTE_ORDER=LITTLE_ENDIAN' "$CHECK_DIR/static/abi.txt" >/dev/null; then
  echo 'Static ThinLTO ABI detection no longer reproduces; review this regression test.' >&2
  exit 1
fi

# Linking is allowed; no cross-compiled executable is ever run on the host.
"$CMAKE" "${common[@]}" -B "$CHECK_DIR/linked" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=EXECUTABLE -DREQUIRE_ABI=ON \
  > "$CHECK_DIR/linked.log" 2>&1 || { cat "$CHECK_DIR/linked.log"; exit 1; }
"$CMAKE" --build "$CHECK_DIR/linked" --parallel 4 \
  >> "$CHECK_DIR/linked.log" 2>&1 || { cat "$CHECK_DIR/linked.log"; exit 1; }
for line in C_BYTE_ORDER=LITTLE_ENDIAN CXX_BYTE_ORDER=LITTLE_ENDIAN C_ABI=ELF CXX_ABI=ELF C_PTR=8 CXX_PTR=8 SSIZE_T=8; do
  grep -Fx "$line" "$CHECK_DIR/linked/abi.txt" >/dev/null
done
magic=$(od -An -tx1 -N4 "$CHECK_DIR/linked/CMakeFiles/abi_bitcode.dir/entry.c.o" | tr -d '[:space:]')
test "$magic" = 4243c0de

# Exercise the production verifier against a real generated CMakeCache, not
# only a simplified unit-test cache. This runs before the expensive full build.
"${MPV_TEST_PYTHON:-python3}" - "$ROOT_DIR" "$CHECK_DIR/linked" "$TRIPLE" <<'PY'
import importlib.util
import pathlib
import sys

sys.dont_write_bytecode = True
root, build = map(pathlib.Path, sys.argv[1:3])
spec = importlib.util.spec_from_file_location("lto_verify", root / "scripts/verify-build-lto.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)
count = verify.verify_cmake(build, "CMake ABI fixture", sys.argv[3])
print("Production CMake ThinLTO verifier passed on generated cache (" + str(count) + " target sources)")
PY
echo "CMake ThinLTO ABI regression passed: $TRIPLE (linked probes, target bitcode retained)"
