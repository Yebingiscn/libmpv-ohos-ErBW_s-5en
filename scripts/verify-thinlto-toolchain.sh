#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.."; pwd)
SDK_NATIVE=${1:?Usage: verify-thinlto-toolchain.sh NATIVE_SDK_DIR TARGET_TRIPLE [JOBS]}
TRIPLE=${2:?Missing target triple}
JOBS=${3:-4}
case "$TRIPLE" in aarch64-linux-ohos|x86_64-linux-ohos) ;; *) exit 1 ;; esac
case "$JOBS" in ''|*[!0-9]*) exit 1 ;; esac
[ "$JOBS" -ge 1 ] && [ "$JOBS" -le 64 ]
LLVM="$SDK_NATIVE/llvm/bin"
for tool in clang llvm-ar llvm-ranlib llvm-dis llvm-nm llvm-objdump llvm-readelf ld.lld; do
  test -x "$LLVM/$tool" || { echo "Missing SDK tool: $tool" >&2; exit 1; }
done
CHECK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/sweetvideo-thinlto.XXXXXX")
cleanup() {
  rm -f -- "$CHECK_DIR/leaf.o" "$CHECK_DIR/entry.o" "$CHECK_DIR/libleaf.a" \
    "$CHECK_DIR/libprobe.so" "$CHECK_DIR/leaf.ll" "$CHECK_DIR/entry.asm"
  rmdir -- "$CHECK_DIR"
}
trap cleanup EXIT
flags=("--target=$TRIPLE" "--sysroot=$SDK_NATIVE/sysroot" -O3 -fPIC -flto=thin)
"$LLVM/clang" "${flags[@]}" -c "$ROOT_DIR/tests/thinlto/leaf.c" -o "$CHECK_DIR/leaf.o"
"$LLVM/clang" "${flags[@]}" -c "$ROOT_DIR/tests/thinlto/entry.c" -o "$CHECK_DIR/entry.o"
"$LLVM/llvm-dis" "$CHECK_DIR/leaf.o" -o "$CHECK_DIR/leaf.ll"
grep -Eq '^\^0 = module:' "$CHECK_DIR/leaf.ll" || { echo 'ThinLTO module summary missing' >&2; exit 1; }
"$LLVM/llvm-ar" rcs "$CHECK_DIR/libleaf.a" "$CHECK_DIR/leaf.o"
"$LLVM/llvm-ranlib" "$CHECK_DIR/libleaf.a"
"$LLVM/clang" "${flags[@]}" -fuse-ld=lld -shared -Wl,-z,defs \
  "-Wl,--thinlto-jobs=$JOBS" "$CHECK_DIR/entry.o" "$CHECK_DIR/libleaf.a" -o "$CHECK_DIR/libprobe.so"
symbols=$("$LLVM/llvm-nm" "$CHECK_DIR/libprobe.so")
grep -Eq '[[:space:]]thinlto_entry$' <<< "$symbols"
"$LLVM/llvm-objdump" -d --disassemble-symbols=thinlto_entry "$CHECK_DIR/libprobe.so" > "$CHECK_DIR/entry.asm"
# LLD can retain an unused hidden function body without --gc-sections. Check
# the caller's actual machine code, not the presence of that harmless body.
if grep -F thinlto_leaf "$CHECK_DIR/entry.asm" >/dev/null; then
  echo 'ThinLTO cross-module import/inlining probe failed' >&2
  cat "$CHECK_DIR/entry.asm" >&2
  exit 1
fi
header=$("$LLVM/llvm-readelf" -h "$CHECK_DIR/libprobe.so")
case "$TRIPLE" in
  aarch64-*) grep -F AArch64 <<< "$header" >/dev/null ;;
  x86_64-*) grep -F X86-64 <<< "$header" >/dev/null ;;
esac
echo "SDK ThinLTO compile/archive/ranlib/link/import probe passed: $TRIPLE (jobs=$JOBS)"
