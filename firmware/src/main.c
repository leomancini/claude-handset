/*
 * Claude Handset firmware.
 *
 * USB composite device: UAC2 speaker + microphone, boot-protocol HID keyboard
 * (F13 / F14 on the two side buttons), and a picotool reset interface.
 * Everything runs from a single cooperative main loop; audio moves through
 * PIO and DMA in the background.
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tusb.h"

#include "usb_audio.h"
#include "buttons.h"
#include "diag.h"

#if HANDSET_HAS_PWRGD
static void pwrgd_init(void) {
  gpio_init(HANDSET_PIN_PWRGD);
  gpio_set_dir(HANDSET_PIN_PWRGD, GPIO_IN);
  gpio_set_pulls(HANDSET_PIN_PWRGD, false, false);   // 10 k pull-up on the board
}

static void pwrgd_task(void) {
  static uint32_t next_us;
  uint32_t now = time_us_32();
  if ((int32_t) (now - next_us) < 0) return;
  next_us = now + 1000;
  usb_audio_set_power_good(gpio_get(HANDSET_PIN_PWRGD));
}
#else
static void pwrgd_init(void) {}
static void pwrgd_task(void) {}
#endif

int main(void) {
  // Watchdog + crash breadcrumbs; see diag.h for the reboot policy.
  diag_boot();

  DIAG_CRUMB(1);
  pwrgd_init();
  buttons_init();

  // USB first so the host sees the device as early as possible.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  DIAG_CRUMB(2);
  tusb_init(0, &dev_init);
  watchdog_update();

  DIAG_CRUMB(3);
  usb_audio_init();

  while (true) {
    DIAG_LOOP();
    DIAG_CRUMB(10); tud_task();
    DIAG_CRUMB(11); usb_audio_speaker_task();
    DIAG_CRUMB(12); usb_audio_mic_task();
    DIAG_CRUMB(13); buttons_task();
    DIAG_CRUMB(14); pwrgd_task();
    DIAG_CRUMB(15); diag_task();
    watchdog_update();
  }
}
