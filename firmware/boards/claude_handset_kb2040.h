/*
 * Bench variant: the same firmware on an Adafruit KB2040 (8 MB flash) with
 * the handset peripherals wired to the same GPIO numbers. Used to prove the
 * USB stack on a Pico-class board when the real handset is not to hand.
 *
 * Differences from the real board: no LDO power-good signal, and the buttons
 * need the RP2040's internal pull-ups.
 *
 * Build with -DPICO_BOARD=claude_handset_kb2040.
 */

#ifndef _BOARDS_CLAUDE_HANDSET_KB2040_H
#define _BOARDS_CLAUDE_HANDSET_KB2040_H

pico_board_cmake_set(PICO_PLATFORM, rp2040)
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (8 * 1024 * 1024))

#include "boards/adafruit_kb2040.h"

#define CLAUDE_HANDSET_KB2040

#define HANDSET_PIN_I2S_BCLK   2
#define HANDSET_PIN_I2S_LRCLK  3
#define HANDSET_PIN_I2S_DATA   4
#define HANDSET_PIN_PDM_DATA   5
#define HANDSET_PIN_PDM_CLK    6
#define HANDSET_PIN_BTN_F13    7
#define HANDSET_PIN_BTN_F14    8
#define HANDSET_PIN_PWRGD      9
#define HANDSET_PIN_AMP_SD     21

#define HANDSET_BTN_INTERNAL_PULLUP 1
#define HANDSET_HAS_PWRGD           0

#endif
