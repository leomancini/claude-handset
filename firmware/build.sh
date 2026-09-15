#!/bin/sh
# Build the handset firmware.
#
#   ./build.sh                       -> build/claude_handset/claude_handset.uf2
#   ./build.sh claude_handset_kb2040 -> bench build for an Adafruit KB2040
#
# Expects the Pico SDK and an arm-none-eabi toolchain. Defaults match the
# layout the Raspberry Pi VS Code extension uses (~/.pico-sdk/...); override
# with PICO_SDK_PATH and PICO_TOOLCHAIN_PATH.
set -eu
cd "$(dirname "$0")"

BOARD="${1:-claude_handset}"

export PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/.pico-sdk/sdk/2.3.1}"
if [ -z "${PICO_TOOLCHAIN_PATH:-}" ]; then
  for d in "$HOME"/.pico-sdk/toolchain/*/bin; do
    [ -x "$d/arm-none-eabi-gcc" ] && export PICO_TOOLCHAIN_PATH="$d" && break
  done
fi
[ -n "${PICO_TOOLCHAIN_PATH:-}" ] && export PATH="$PICO_TOOLCHAIN_PATH:$PATH"

BUILD="build/$BOARD"
cmake -S . -B "$BUILD" -G Ninja -DPICO_BOARD="$BOARD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"

echo
echo "UF2: $BUILD/claude_handset.uf2"
