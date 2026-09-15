/*
 * Side buttons SW1 and SW2: debounced GPIO inputs feeding a boot keyboard
 * HID report (F17 / F18 by default, see buttons.c).
 */

#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdbool.h>
#include <stdint.h>

void buttons_init(void);

// Call from the main loop. Samples the buttons at 1 kHz, debounces them, and
// sends a keyboard report whenever the pressed set changes.
void buttons_task(void);

// Current debounced state.
bool button_sw1_pressed(void);
bool button_sw2_pressed(void);

#endif
