/*
 * Raspberry Pi "reset" vendor interface, so `picotool load -f` and
 * `picotool reboot -f` work while this firmware is running (the board is
 * meant to live inside an enclosure where SW3/SW4 are hard to reach).
 *
 * Mirrors the SDK's pico_usb_reset implementation: a TinyUSB application
 * class driver that claims the bare vendor interface and answers two control
 * requests, RESET_REQUEST_BOOTSEL (1) and RESET_REQUEST_FLASH (2).
 */

#include "tusb.h"
#include "device/usbd_pvt.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

#include "usb_descriptors.h"

#define RESET_INTERFACE_SUBCLASS  0x00
#define RESET_INTERFACE_PROTOCOL  0x01
#define RESET_REQUEST_BOOTSEL     0x01
#define RESET_REQUEST_FLASH       0x02

static uint8_t reset_itf_num = 0xFF;

static void reset_driver_init(void) {}

static bool reset_driver_deinit(void) { return true; }

static void reset_driver_reset(uint8_t rhport) {
  (void) rhport;
  reset_itf_num = 0xFF;
}

static uint16_t reset_driver_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) rhport;
  TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass &&
            RESET_INTERFACE_SUBCLASS == itf_desc->bInterfaceSubClass &&
            RESET_INTERFACE_PROTOCOL == itf_desc->bInterfaceProtocol, 0);
  uint16_t const drv_len = sizeof(tusb_desc_interface_t);
  TU_VERIFY(max_len >= drv_len, 0);
  reset_itf_num = itf_desc->bInterfaceNumber;
  return drv_len;
}

static bool reset_driver_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  (void) rhport;
  if (stage != CONTROL_STAGE_SETUP) return true;
  if (request->wIndex != reset_itf_num) return false;

  if (request->bRequest == RESET_REQUEST_BOOTSEL) {
    // Low two bits of wValue select which bootloader interfaces to disable.
    // No activity LED on the handset.
    rom_reset_usb_boot(0, request->wValue & 0x3u);
  }
  if (request->bRequest == RESET_REQUEST_FLASH) {
    watchdog_reboot(0, 0, 100);
    return true;
  }
  return false;
}

static bool reset_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) rhport; (void) ep_addr; (void) result; (void) xferred_bytes;
  return true;
}

static usbd_class_driver_t const reset_driver = {
    .name            = "RPI-RESET",
    .init            = reset_driver_init,
    .deinit          = reset_driver_deinit,
    .reset           = reset_driver_reset,
    .open            = reset_driver_open,
    .control_xfer_cb = reset_driver_control_xfer_cb,
    .xfer_cb         = reset_driver_xfer_cb,
    .sof             = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = 1;
  return &reset_driver;
}
