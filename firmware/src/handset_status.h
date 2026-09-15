/*
 * Live status readable over the vendor "reset" interface with a control
 * request (IN, vendor, interface; bRequest = HANDSET_REQUEST_STATUS). Used by
 * tools/handset-status.c. Keep the layout in sync with that tool.
 */

#ifndef HANDSET_STATUS_H_
#define HANDSET_STATUS_H_

#include <stdint.h>

#define HANDSET_REQUEST_STATUS 0x10
#define HANDSET_STATUS_MAGIC   0x48534554u   // "HSET"

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t uptime_ms;
  uint32_t loops;
  uint8_t  spk_streaming;
  uint8_t  mic_streaming;
  uint8_t  spk_mute;
  uint8_t  mic_mute;
  int16_t  spk_volume;        // 1/256 dB
  int16_t  mic_volume;        // 1/256 dB
  uint8_t  amp_gpio;          // SD_MODE pin level
  uint8_t  power_good;
  uint8_t  spk_primed;
  uint8_t  pad0;
  uint16_t usb_out_fifo;      // bytes waiting in the speaker FIFO
  uint16_t usb_in_fifo;       // bytes waiting in the mic FIFO
  uint16_t i2s_queued;        // frames queued ahead of the I2S DMA reader
  uint16_t pad1;
  uint32_t spk_usb_frames;    // frames taken from USB since boot
  uint32_t spk_silence_frames;
  uint32_t spk_underruns;
  uint32_t mic_samples;
  uint32_t i2s_dma_read_addr;
  uint32_t pdm_dma_write_addr;
  uint32_t pio_i2s_pc;        // current PIO program counter, I2S state machine
  uint32_t pio_pdm_pc;
  int32_t  spk_gain_q15;
  int32_t  mic_gain_q15;
} handset_status_t;

#endif
