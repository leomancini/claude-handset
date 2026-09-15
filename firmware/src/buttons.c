/*
 * Side buttons -> HID keyboard.
 *
 * The board was designed around F13 / F14, but macOS binds F14 and F15 to
 * display brightness on keyboards without dedicated keys, so the firmware
 * sends F17 (SW1) and F18 (SW2), which have no default binding on macOS,
 * Windows or Linux. Change HANDSET_KEY_SW1 / HANDSET_KEY_SW2 to taste.
 */

#include "buttons.h"

#include "pico/stdlib.h"
#include "tusb.h"

#ifndef HANDSET_KEY_SW1
#define HANDSET_KEY_SW1    HID_KEY_F17
#endif
#ifndef HANDSET_KEY_SW2
#define HANDSET_KEY_SW2    HID_KEY_F18
#endif

#define BTN_SAMPLE_US      1000u   // 1 kHz sampling
#define BTN_DEBOUNCE_TICKS 10u     // 10 ms stable before a change is accepted

typedef struct {
  uint     pin;
  uint8_t  keycode;
  bool     stable;     // debounced pressed state
  bool     last_raw;
  uint8_t  ticks;      // consecutive samples that disagree with `stable`
} button_t;

static button_t buttons[] = {
  { .pin = HANDSET_PIN_BTN_F13, .keycode = HANDSET_KEY_SW1 },
  { .pin = HANDSET_PIN_BTN_F14, .keycode = HANDSET_KEY_SW2 },
};
#define N_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

static uint32_t next_sample_us;
static bool     report_dirty;

void buttons_init(void) {
  for (size_t i = 0; i < N_BUTTONS; i++) {
    gpio_init(buttons[i].pin);
    gpio_set_dir(buttons[i].pin, GPIO_IN);
#if HANDSET_BTN_INTERNAL_PULLUP
    gpio_pull_up(buttons[i].pin);
#else
    // 10 k pull-ups on the board; never enable the internal pull-downs.
    gpio_set_pulls(buttons[i].pin, false, false);
#endif
    buttons[i].stable = false;
    buttons[i].last_raw = false;
    buttons[i].ticks = 0;
  }
  next_sample_us = time_us_32();
  report_dirty = true;   // send an all-released report once the host is up
}

static void send_report(void) {
  uint8_t keys[6] = { 0 };
  int n = 0;
  for (size_t i = 0; i < N_BUTTONS; i++) {
    if (buttons[i].stable) keys[n++] = buttons[i].keycode;
  }
  tud_hid_keyboard_report(0, 0, keys);
}

void buttons_task(void) {
  uint32_t now = time_us_32();
  if ((int32_t) (now - next_sample_us) >= 0) {
    next_sample_us += BTN_SAMPLE_US;
    for (size_t i = 0; i < N_BUTTONS; i++) {
      button_t *b = &buttons[i];
      bool raw = !gpio_get(b->pin);   // active low
      if (raw != b->stable) {
        if (b->ticks < 255) b->ticks++;
        if (b->ticks >= BTN_DEBOUNCE_TICKS) {
          b->stable = raw;
          b->ticks = 0;
          report_dirty = true;
        }
      } else {
        b->ticks = 0;
      }
      b->last_raw = raw;
    }
  }

  if (!report_dirty) return;

  // A press while the host has us suspended should wake it.
  if (tud_suspended()) {
    tud_remote_wakeup();
    return;
  }

  if (tud_hid_ready()) {
    send_report();
    report_dirty = false;
  }
}

bool button_sw1_pressed(void) { return buttons[0].stable; }
bool button_sw2_pressed(void) { return buttons[1].stable; }

//--------------------------------------------------------------------+
// HID class callbacks
//--------------------------------------------------------------------+

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
  // Keyboard LED output report (caps lock etc). Nothing to drive.
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}
