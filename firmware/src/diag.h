/*
 * Crash diagnostics.
 *
 * The firmware runs under a 2 s watchdog. Breadcrumbs (DIAG_CRUMB) and a
 * hard-fault handler record where execution was in the watchdog scratch
 * registers, which survive the watchdog reset. On the next boot:
 *
 *   - first watchdog reboot in a row: boot normally, but append the recorded
 *     breadcrumb, fault PC and loop counter to the USB serial string
 *     ("<uid>-WDT<stage>-<pc>-<loops>", all hex) so it can be read with
 *     ioreg. Stage bit 0x8000 = hard fault (pc = stacked PC); bit 0x4000 =
 *     panic() (pc = format string address, loops = caller address);
 *   - second in a row: drop into the ROM bootloader (RPI-RP2) so the board
 *     can always be reflashed over USB.
 *
 * Scratch usage: [0] breadcrumb, [1] main-loop counter, [2] fault PC,
 * [3] consecutive watchdog reboots. Scratch 4-7 belong to the SDK.
 */

#ifndef DIAG_H_
#define DIAG_H_

#include <stdbool.h>
#include <stdint.h>

#include "hardware/structs/watchdog.h"

#define DIAG_CRUMB(n)   (watchdog_hw->scratch[0] = (n))
#define DIAG_LOOP()     (watchdog_hw->scratch[1]++)

// Call first thing in main(). Handles the watchdog-reboot policy above and
// enables the watchdog.
void diag_boot(void);

// Call from the main loop; clears the consecutive-reboot counter once the
// firmware has stayed up for a while.
void diag_task(void);

// Text to append to the USB serial number, or "" when the last boot was clean.
const char *diag_serial_suffix(void);

#endif
