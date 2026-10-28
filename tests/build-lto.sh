#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Configuration-only tests need no target SDK, including under Git Bash.
uname() { echo Linux; }
export -f uname
for arch in arm64 x86_64; do
  (
    export MPV_BUILD_ARCH=$arch MPV_LTO_JOBS=3
    . ./env.sh
    . ./scripts/build-lto.sh
    [[ "$CFLAGS $CXXFLAGS" != *-flto* ]]
    [[ "$AR" == *'/llvm/bin/llvm-ar' ]]
    [[ "$RANLIB" == *'/llvm/bin/llvm-ranlib' ]]
    test "${FFMPEG_LTO_ARGS[*]}" = '--enable-lto=thin'
    test "${MPV_LTO_ARGS[*]}" = '-Db_lto=true -Db_lto_mode=thin -Db_lto_threads=3'
    test "${MESON_LTO_ARGS[*]}" = "${MPV_LTO_ARGS[*]}"
    test "$FFMPEG_LTO_LDFLAGS" = '-fuse-ld=lld -Wl,--thinlto-jobs=3'
    [[ "$MAKE_LTO_CFLAGS" == *' -flto=thin' ]]
    [[ "$MAKE_LTO_LDFLAGS" == *'-flto=thin -fuse-ld=lld -Wl,--thinlto-jobs=3' ]]
    for flag in "-DCMAKE_C_FLAGS=$CFLAGS -flto=thin" \
                "-DCMAKE_CXX_FLAGS=$CXXFLAGS -flto=thin" \
                "-DCMAKE_AR=$AR" "-DCMAKE_RANLIB=$RANLIB" \
                -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; do
      found=false
      for option in "${CMAKE_LTO_ARGS[@]}"; do
        if [ "$option" = "$flag" ]; then found=true; break; fi
      done
      test "$found" = true
    done
    echo "$arch default ThinLTO flags passed"
  )
done
(
  unset MPV_LTO_JOBS
  . ./env.sh
  test "$MPV_LTO_JOBS" = 4
)
for jobs in 0 65 all '-1' '2;echo unsafe'; do
  if (export MPV_LTO_JOBS=$jobs; . ./env.sh) >/dev/null 2>&1; then
    echo "Invalid LTO jobs was accepted: $jobs" >&2; exit 1
  fi
done
CHECK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/sweetvideo-lto-flags.XXXXXX")
cleanup() {
  rm -f -- "$CHECK_DIR/.sweetvideo-lto-mode" "$CHECK_DIR/ffbuild/config.mak" \
    "$CHECK_DIR/meson-private/coredata.dat" "$CHECK_DIR/CMakeCache.txt" \
    "$CHECK_DIR/source/stale.o" "$CHECK_DIR/.sweetvideo-make-lto-inputs" "$CHECK_DIR/probe-called"
  rmdir -- "$CHECK_DIR/ffbuild" "$CHECK_DIR/meson-private" "$CHECK_DIR/source" "$CHECK_DIR"
}
trap cleanup EXIT
. ./env.sh
. ./scripts/build-lto.sh
# A guard must fail before probing: absent SDKs must not make this a false
# positive. Successful preparations below use this deliberately cheap stub.
bash() { touch "$CHECK_DIR/probe-called"; return 0; }
make() { command "${MPV_TEST_MAKE:-make}" "$@"; }
mkdir "$CHECK_DIR/ffbuild" "$CHECK_DIR/meson-private" "$CHECK_DIR/source"
printf '%s\n' "different-arch:thin:$OHOS_NDK_HOME:all-native-v1" > "$CHECK_DIR/.sweetvideo-lto-mode"
if prepare_lto_build fixture "$CHECK_DIR" >/dev/null 2>&1; then
  echo 'Changing target architecture reused cached objects' >&2; exit 1
fi
test ! -e "$CHECK_DIR/probe-called"
printf '%s\n' "$MPV_BUILD_ARCH:thin:$OHOS_NDK_HOME" > "$CHECK_DIR/.sweetvideo-lto-mode"
if prepare_lto_build fixture "$CHECK_DIR" >/dev/null 2>&1; then
  echo 'Earlier FFmpeg/mpv-only cache was accepted' >&2; exit 1
fi
test ! -e "$CHECK_DIR/probe-called"
rm -f -- "$CHECK_DIR/.sweetvideo-lto-mode"
for cache in ffbuild/config.mak meson-private/coredata.dat CMakeCache.txt; do
  touch "$CHECK_DIR/$cache"
  if prepare_lto_build fixture "$CHECK_DIR" >/dev/null 2>&1; then
    echo "Untracked cache was accepted for ThinLTO: $cache" >&2; exit 1
  fi
  test ! -e "$CHECK_DIR/probe-called"
  rm -f -- "$CHECK_DIR/$cache"
done
touch "$CHECK_DIR/source/stale.o"
if prepare_lto_build fixture "$CHECK_DIR" "$CHECK_DIR/source" >/dev/null 2>&1; then
  echo 'Untracked Make objects were accepted for ThinLTO' >&2; exit 1
fi
test ! -e "$CHECK_DIR/probe-called"
rm -f -- "$CHECK_DIR/source/stale.o"
prepare_lto_build fixture "$CHECK_DIR" "$CHECK_DIR/source"
test -f "$CHECK_DIR/probe-called"
test "$(< "$CHECK_DIR/.sweetvideo-lto-mode")" = "$MPV_BUILD_ARCH:thin:$OHOS_NDK_HOME:all-native-v1"

record_make_lto_inputs "$CHECK_DIR" "$ROOT_DIR/tests/thinlto" -f make-inputs.mk \
  "CC=fixture-clang --target=$TARGET_TRIPLE" "CFLAGS=$MAKE_LTO_CFLAGS" \
  "LDFLAGS=$MAKE_LTO_LDFLAGS" "AR=fixture-llvm-ar rcu" "RANLIB=fixture-llvm-ranlib"
grep -Fx "CC=fixture-clang --target=$TARGET_TRIPLE" "$CHECK_DIR/.sweetvideo-make-lto-inputs" >/dev/null
grep -Fx "CFLAGS=$MAKE_LTO_CFLAGS" "$CHECK_DIR/.sweetvideo-make-lto-inputs" >/dev/null
grep -Fx "LDFLAGS=$MAKE_LTO_LDFLAGS" "$CHECK_DIR/.sweetvideo-make-lto-inputs" >/dev/null
grep -Fx "AR=fixture-llvm-ar rcu" "$CHECK_DIR/.sweetvideo-make-lto-inputs" >/dev/null
# The SDK ABI fixture also exercises this mode. Do not reintroduce a later
# nghttp3-specific STATIC_LIBRARY override that hides layout markers in IR.
grep -F -- '-DCMAKE_TRY_COMPILE_TARGET_TYPE=EXECUTABLE' scripts/nghttp3.sh >/dev/null
if grep -Eq '^[[:space:]]*-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY' scripts/nghttp3.sh; then
  echo 'nghttp3 must link CMake ABI probes when using ThinLTO' >&2; exit 1
fi
echo 'ThinLTO defaults, resource bounds, Make inputs and mixed-cache guards passed'
