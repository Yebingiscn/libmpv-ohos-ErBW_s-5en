#!/bin/bash
# Source after env.sh. Apply these explicitly to target C/C++ builds only:
# Rust, host generators, system libraries and assembly keep their own flags.

FFMPEG_LTO_ARGS=(--enable-lto=thin)
FFMPEG_LTO_LDFLAGS="-fuse-ld=lld -Wl,--thinlto-jobs=$MPV_LTO_JOBS"
MESON_LTO_ARGS=(-Db_lto=true -Db_lto_mode=thin "-Db_lto_threads=$MPV_LTO_JOBS")
MPV_LTO_ARGS=("${MESON_LTO_ARGS[@]}")
MAKE_LTO_CFLAGS="$CFLAGS -flto=thin"
MAKE_LTO_LDFLAGS="${LDFLAGS:+$LDFLAGS }-flto=thin -fuse-ld=lld -Wl,--thinlto-jobs=$MPV_LTO_JOBS"
CMAKE_LTO_ARGS=(
  "-DCMAKE_C_FLAGS=$CFLAGS -flto=thin"
  "-DCMAKE_CXX_FLAGS=$CXXFLAGS -flto=thin"
  "-DCMAKE_EXE_LINKER_FLAGS=$MAKE_LTO_LDFLAGS"
  "-DCMAKE_SHARED_LINKER_FLAGS=$MAKE_LTO_LDFLAGS"
  "-DCMAKE_MODULE_LINKER_FLAGS=$MAKE_LTO_LDFLAGS"
  "-DCMAKE_AR=$AR" "-DCMAKE_RANLIB=$RANLIB" "-DCMAKE_NM=$NM"
  "-DCMAKE_C_COMPILER_AR=$AR" "-DCMAKE_CXX_COMPILER_AR=$AR"
  "-DCMAKE_C_COMPILER_RANLIB=$RANLIB" "-DCMAKE_CXX_COMPILER_RANLIB=$RANLIB"
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

prepare_lto_build() {
  local component=$1 build_dir=$2
  local stamp="$build_dir/.sweetvideo-lto-mode"
  # The scope version also rejects earlier FFmpeg/mpv-only caches. Reusing
  # their installed dependencies would silently mix native and bitcode files.
  local mode="$MPV_BUILD_ARCH:thin:$OHOS_NDK_HOME:all-native-v1"
  mkdir -p "$build_dir"
  if [ -f "$stamp" ]; then
    if [ "$(< "$stamp")" != "$mode" ]; then
      echo "$component build mode/SDK changed. Use a separate fresh checkout; refusing mixed objects." >&2
      return 1
    fi
  elif [ -f "$build_dir/ffbuild/config.mak" ] ||
       [ -f "$build_dir/meson-private/coredata.dat" ] ||
       [ -f "$build_dir/CMakeCache.txt" ]; then
    echo "$component has untracked build caches. Use a fresh checkout for ThinLTO." >&2
    return 1
  elif [ -n "${3:-}" ] &&
       [ -n "$(find "$3" -type f \( -name '*.o' -o -name '*.a' \) -print -quit)" ]; then
    echo "$component has untracked in-source objects. Use a fresh checkout for ThinLTO." >&2
    return 1
  fi
  bash "$ROOT_DIR/scripts/verify-thinlto-toolchain.sh" \
    "$OHOS_NDK_HOME/native" "$TARGET_TRIPLE" "$MPV_LTO_JOBS"
  printf '%s\n' "$mode" > "$stamp"
}

# Expand the same Make command-line variables used for the real build, inside
# the library's own Makefile. Do not dump the full Make database/environment:
# it may include credentials and thousands of unrelated host variables.
record_make_lto_inputs() {
  local build_dir=$1 source_dir=$2
  shift 2
  make --no-print-directory -s -C "$source_dir" "$@" \
    --eval='sweetvideo-lto-inputs: ; @printf "%s\n" "CC=$(CC)" "CFLAGS=$(CFLAGS)" "LDFLAGS=$(LDFLAGS)" "AR=$(AR)" "RANLIB=$(RANLIB)"' \
    sweetvideo-lto-inputs > "$build_dir/.sweetvideo-make-lto-inputs"
}
