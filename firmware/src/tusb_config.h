/*
 * TinyUSB configuration for the Claude Handset.
 *
 * One USB 2.0 full-speed composite device:
 *   - UAC2 audio function: 48 kHz / 16-bit stereo speaker (asynchronous OUT
 *     endpoint with explicit feedback) and 48 kHz / 16-bit mono microphone.
 *   - Boot-protocol HID keyboard for the two side buttons (F13 / F14).
 *   - Raspberry Pi "reset" vendor interface so picotool can reboot the board
 *     into BOOTSEL without touching the buttons.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "usb_descriptors.h"

//--------------------------------------------------------------------
// Common
//--------------------------------------------------------------------

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG              0
#endif

#define CFG_TUD_ENABLED             1
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// Device
//--------------------------------------------------------------------

#define CFG_TUD_ENDPOINT0_SIZE      64

#define CFG_TUD_AUDIO               1
#define CFG_TUD_HID                 1
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

//--------------------------------------------------------------------
// Audio (UAC2)
//--------------------------------------------------------------------

#define HANDSET_SAMPLE_RATE                             48000
#define HANDSET_SPK_CHANNELS                            2
#define HANDSET_MIC_CHANNELS                            1
#define HANDSET_BYTES_PER_SAMPLE                        2
#define HANDSET_BITS_PER_SAMPLE                         16

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                   HANDSET_AUDIO_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                   2   // speaker + microphone streaming interfaces
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                64

#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP               0
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION 1   // full speed: send 10.14 in 3 bytes per the USB spec
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL                1

// Microphone: EP IN
#define CFG_TUD_AUDIO_ENABLE_EP_IN                      1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX               TUD_AUDIO_EP_SIZE(HANDSET_SAMPLE_RATE, HANDSET_BYTES_PER_SAMPLE, HANDSET_MIC_CHANNELS)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ            (8 * CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)

// Speaker: EP OUT. The FIFO-count feedback method regulates this buffer to
// half full, so 8 packets gives ~4 ms of buffering against USB jitter.
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                     1
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX              TUD_AUDIO_EP_SIZE(HANDSET_SAMPLE_RATE, HANDSET_BYTES_PER_SAMPLE, HANDSET_SPK_CHANNELS)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ           (8 * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX)

//--------------------------------------------------------------------
// HID
//--------------------------------------------------------------------

#define CFG_TUD_HID_EP_BUFSIZE      8

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
