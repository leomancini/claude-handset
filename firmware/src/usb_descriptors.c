/*
 * USB descriptors for the Claude Handset composite device.
 */

#include <string.h>

#include "tusb.h"
#include "pico/unique_id.h"
#include "usb_descriptors.h"
#include "diag.h"

//--------------------------------------------------------------------+
// Device descriptor
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Interface Association Descriptors need the "miscellaneous / common /
    // IAD" device class triple.
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = HANDSET_USB_VID,
    .idProduct          = HANDSET_USB_PID,
    .bcdDevice          = HANDSET_USB_BCD,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID report descriptor: plain boot-compatible keyboard, no report ID
//--------------------------------------------------------------------+

static uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void) instance;
  return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration descriptor
//--------------------------------------------------------------------+

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + HANDSET_AUDIO_DESC_LEN + TUD_HID_DESC_LEN + HANDSET_RESET_DESC_LEN)

// Bus powered. The MAX98357A can draw a few hundred mA at full volume, so ask
// the host for the full 500 mA the PTC fuse allows.
#define HANDSET_POWER_MA 500

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, HANDSET_POWER_MA),

    HANDSET_AUDIO_DESCRIPTOR(STRID_SPEAKER, STRID_MICROPHONE),

    // Boot keyboard, 8-byte reports, polled every 10 ms
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), EPNUM_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10),

    HANDSET_RESET_DESCRIPTOR(ITF_NUM_RESET, STRID_RESET),
};

TU_VERIFY_STATIC(sizeof(desc_configuration) == CONFIG_TOTAL_LEN, "configuration descriptor length mismatch");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String descriptors
//--------------------------------------------------------------------+

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: English (0x0409)
    "Leo Mancini",                  // 1: Manufacturer
    "Claude Handset",               // 2: Product
    NULL,                           // 3: Serial, from the flash unique ID
    "Claude Handset Speaker",       // 4: Speaker streaming interface
    "Claude Handset Microphone",    // 5: Microphone streaming interface
    "Claude Handset Buttons",       // 6: HID interface
    "Reset",                        // 7: picotool reset interface
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  if (index == STRID_LANGID) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else if (index == STRID_SERIAL) {
    char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1 + 48];
    pico_get_unique_board_id_string(serial, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1);
    strncat(serial, diag_serial_suffix(), sizeof(serial) - strlen(serial) - 1);
    chr_count = strlen(serial);
    for (size_t i = 0; i < chr_count; i++) _desc_str[1 + i] = (uint16_t) serial[i];
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
    const char *str = string_desc_arr[index];
    if (str == NULL) return NULL;

    chr_count = strlen(str);
    size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
    if (chr_count > max_count) chr_count = max_count;

    for (size_t i = 0; i < chr_count; i++) _desc_str[1 + i] = (uint16_t) str[i];
  }

  // First byte is total length (including the header), second is the type.
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
