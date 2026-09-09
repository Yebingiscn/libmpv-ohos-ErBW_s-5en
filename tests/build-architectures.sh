#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."
# Configuration-only tests also run under Git Bash without a Linux SDK.
uname() { echo Linux; }
export -f uname

for arch in arm64 x86_64; do
  (
    export TARGET_ARCH=$arch
    . ./env.sh
    test "$DEST" = "$ROOT_DIR/libmpv/$arch-build"
    test "$CROSS_FILE" = "$ROOT_DIR/libmpv/$arch-crossfile.ini"
    if [ "$arch" = arm64 ]; then
      test "$TARGET_TRIPLE" = aarch64-linux-ohos
      test "$RUST_TARGET" = aarch64-unknown-linux-ohos
      test "$OHOS_ARCH" = arm64-v8a
      test "$FFMPEG_CPU" = armv8-a
      test "$ARCHIVE_ARCH" = aarch64
    else
      test "$TARGET_TRIPLE" = x86_64-linux-ohos
      test "$RUST_TARGET" = x86_64-unknown-linux-ohos
      test "$OHOS_ARCH" = x86_64
      test "$FFMPEG_CPU" = x86-64
      test "$ARCHIVE_ARCH" = x86_64
    fi
    [[ "$CC" == *"--target=$TARGET_TRIPLE "* ]]
    cross=$(sed -e "s|@OHOS_NDK_HOME@|$OHOS_NDK_HOME|g" \
                -e "s|@TARGET_TRIPLE@|$TARGET_TRIPLE|g" \
                -e "s|@CPU_FAMILY@|$FFMPEG_ARCH|g" \
                -e "s|@CPU@|$OHOS_ARCH|g" crossfiles/ohos-crossfile.ini.in)
    ! grep -q '@' <<< "$cross"
    grep -F "cpu_family = '$FFMPEG_ARCH'" <<< "$cross" >/dev/null
    grep -F -- "--target=$TARGET_TRIPLE" <<< "$cross" >/dev/null
    echo "$arch configuration passed"
  )
done
(
  unset TARGET_ARCH
  . ./env.sh
  test "$TARGET_ARCH" = arm64
)
if (export TARGET_ARCH=invalid; . ./env.sh) >/dev/null 2>&1; then
  echo 'Invalid architecture was accepted' >&2
  exit 1
fi
echo 'Default and invalid architecture checks passed'
