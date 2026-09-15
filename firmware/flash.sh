#!/bin/sh
# Flash the handset over USB with picotool.
#
#   ./flash.sh                       -> flashes build/claude_handset/claude_handset.uf2
#   ./flash.sh claude_handset_kb2040 -> flashes the bench build
#
# Works when the board is in the ROM bootloader (blank flash, or hold SW4
# BOOTSEL while tapping SW3 RESET) and also when this firmware is already
# running, because it exposes the picotool reset interface (-f).
# If the board is in its watchdog fallback (shows as RPI-RP2) this also works.
set -eu
cd "$(dirname "$0")"

BOARD="${1:-claude_handset}"
UF2="build/$BOARD/claude_handset.uf2"
[ -f "$UF2" ] || { echo "missing $UF2, run ./build.sh $BOARD first" >&2; exit 1; }

# picotool only looks for the reset interface on Raspberry Pi's VID unless told
# otherwise, so pass ours explicitly.
picotool load -f --vid 0x1209 --pid 0x0001 -x "$UF2"
