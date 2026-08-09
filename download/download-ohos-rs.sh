#!/bin/bash

set -eu

if command -v rustup &> /dev/null; then
  echo "rustup is already installed"
else
  echo "Installing rustup..."
  wget -qO - https://sh.rustup.rs | sh
fi

rustup target add aarch64-unknown-linux-ohos
# cargo-c is a host tool. Do not let the OHOS target compiler environment
# leak into its vendored C dependencies.
env -u CC -u CXX -u CFLAGS -u CXXFLAGS -u AR -u RANLIB \
  cargo install cargo-c --features=vendored-openssl
