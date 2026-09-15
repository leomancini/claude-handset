/*
 * I2S output to the MAX98357A: PIO state machine fed by a DMA ring buffer.
 *
 * The clocks run continuously from init; when nothing is queued the ring is
 * padded with silence so the amp never sees stale data. The amplifier's
 * shutdown pin is controlled separately (see usb_audio.c).
 */

#ifndef I2S_OUT_H_
#define I2S_OUT_H_

#include <stdbool.h>
#include <stdint.h>

// Ring buffer of stereo frames. 1024 frames = 21 ms at 48 kHz.
#define I2S_RING_FRAMES     1024u
#define I2S_RING_MASK       (I2S_RING_FRAMES - 1u)

void i2s_out_init(uint32_t sample_rate);

// Number of frames queued ahead of the DMA read position.
uint32_t i2s_out_queued(void);

// Number of frames that can be written without overtaking the reader.
uint32_t i2s_out_free(void);

// Queue frames. Each frame is a packed {left, right} int16 pair
// (left in the low half-word). Returns the number of frames accepted.
uint32_t i2s_out_write(const uint32_t *frames, uint32_t count);

// Queue silence.
uint32_t i2s_out_write_silence(uint32_t count);

// Discard everything queued and start again just ahead of the reader.
void i2s_out_reset(void);

#endif
