/*
 * Board definition for the Claude Handset (60 mm round RP2040 board).
 *
 * Hardware (see ../../CLAUDE.md): RP2040 with a 12 MHz crystal and a 2 MB
 * Winbond W25Q16JV QSPI flash, MAX98357A I2S amp, Knowles SPH0641LU4H-1 PDM
 * microphone, two side buttons, MCP1825 3.3 V LDO with power-good output.
 *
 * Pass -DPICO_BOARD=claude_handset (the default in CMakeLists.txt).
 */

#ifndef _BOARDS_CLAUDE_HANDSET_H
#define _BOARDS_CLAUDE_HANDSET_H

// For board detection
pico_board_cmake_set(PICO_PLATFORM, rp2040)
#define CLAUDE_HANDSET

// A fresh crystal circuit: give the oscillator plenty of time to start.
#ifndef PICO_XOSC_STARTUP_DELAY_MULTIPLIER
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64
#endif

// No LED, no UART, no I2C, no SPI broken out. GPIO0/1 are unconnected but
// are still declared as the default UART so the SDK's stdio_uart compiles
// if anyone turns it on for debugging (attach a probe to the bare pads).
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// W25Q16JV behaves like the W25Q080 used on the Pico (same QSPI command set).
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
// Run QSPI at sys_clk / 4 (31 MHz). The Pico uses / 2, but this firmware is
// tiny and executes almost entirely from the XIP cache, so the slower clock
// costs nothing and gives margin on a first-spin layout.
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 4
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

// Boards were assembled in September 2026 with B2 silicon.
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

// ---- Handset pin map (from the netlist, confirmed against the copper) ----
#define HANDSET_PIN_I2S_BCLK   2   // MAX98357A BCLK
#define HANDSET_PIN_I2S_LRCLK  3   // MAX98357A LRCLK (must be BCLK + 1 for the PIO program)
#define HANDSET_PIN_I2S_DATA   4   // MAX98357A DIN
#define HANDSET_PIN_PDM_DATA   5   // SPH0641 DATA
#define HANDSET_PIN_PDM_CLK    6   // SPH0641 CLOCK
#define HANDSET_PIN_BTN_F13    7   // SW1, active low, 10 k external pull-up
#define HANDSET_PIN_BTN_F14    8   // SW2, active low, 10 k external pull-up
#define HANDSET_PIN_PWRGD      9   // LDO power-good, open drain + 10 k pull-up, high = OK
#define HANDSET_PIN_AMP_SD     21  // MAX98357A SD_MODE, high = amp on (left channel)

#define HANDSET_BTN_INTERNAL_PULLUP 0  // pull-ups are on the board
#define HANDSET_HAS_PWRGD           1

#endif
