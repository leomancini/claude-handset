/*
 * Crash diagnostics: watchdog policy, breadcrumbs, hard-fault capture.
 */

#include "diag.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

#define WATCHDOG_TIMEOUT_MS   2000
#define DIAG_STABLE_MS        10000   // uptime after which a hang no longer counts as "consecutive"

static char diag_suffix[48];
static bool diag_armed;

void diag_boot(void) {
  if (watchdog_enable_caused_reboot()) {
    uint32_t count = watchdog_hw->scratch[3] + 1;
    if (count >= 2) {
      rom_reset_usb_boot(0, 0);
    }
    snprintf(diag_suffix, sizeof(diag_suffix), "-WDT%lx-%08lx-%08lx",
             (unsigned long) watchdog_hw->scratch[0],
             (unsigned long) watchdog_hw->scratch[2],
             (unsigned long) watchdog_hw->scratch[1]);
    watchdog_hw->scratch[3] = count;
  } else {
    diag_suffix[0] = '\0';
    watchdog_hw->scratch[3] = 0;
  }
  watchdog_hw->scratch[0] = 0;
  watchdog_hw->scratch[1] = 0;
  watchdog_hw->scratch[2] = 0;
  diag_armed = true;
  watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
}

void diag_task(void) {
  if (diag_armed && to_ms_since_boot(get_absolute_time()) > DIAG_STABLE_MS) {
    watchdog_hw->scratch[3] = 0;
    diag_armed = false;
  }
}

const char *diag_serial_suffix(void) {
  return diag_suffix;
}

//--------------------------------------------------------------------+
// panic(): record the format string and the caller, spin until the
// watchdog bites. Installed via PICO_PANIC_FUNCTION in CMakeLists.txt.
//--------------------------------------------------------------------+

void __attribute__((noreturn, used)) diag_panic(const char *fmt, ...) {
  watchdog_hw->scratch[2] = (uint32_t) fmt;
  watchdog_hw->scratch[1] = (uint32_t) __builtin_return_address(0);
  watchdog_hw->scratch[0] |= 0x4000u;
  while (true) { tight_loop_contents(); }
}

//--------------------------------------------------------------------+
// Hard fault: record the stacked PC, then spin until the watchdog bites.
//--------------------------------------------------------------------+

void __attribute__((used)) diag_hardfault_c(uint32_t *sp) {
  watchdog_hw->scratch[2] = sp[6];   // stacked PC
  watchdog_hw->scratch[0] |= 0x8000u;
  while (true) { tight_loop_contents(); }
}

void __attribute__((naked)) isr_hardfault(void) {
  __asm volatile(
      "movs r0, #4        \n"
      "mov  r1, lr        \n"
      "tst  r0, r1        \n"
      "beq  1f            \n"
      "mrs  r0, psp       \n"
      "b    2f            \n"
      "1: mrs r0, msp     \n"
      "2: ldr r1, =diag_hardfault_c \n"
      "bx   r1            \n"
  );
}
