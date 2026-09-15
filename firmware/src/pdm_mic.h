/*
 * PDM microphone capture and decimation to 16-bit PCM.
 *
 * PIO clocks the SPH0641 at 64 x the output sample rate (3.072 MHz for
 * 48 kHz) and DMA streams the bit stream into a ring buffer. pdm_mic_read()
 * decimates whatever has arrived:
 *
 *   PDM 3.072 MHz --> sinc^4 (CIC order 4), decimate by 32 --> 96 kHz
 *              --> DC blocker --> 47-tap half-band FIR, decimate by 2 --> 48 kHz
 *
 * The CIC stage is evaluated with per-byte lookup tables, so the cost is
 * 16 table reads per 96 kHz sample regardless of the bit rate.
 */

#ifndef PDM_MIC_H_
#define PDM_MIC_H_

#include <stdbool.h>
#include <stdint.h>

void pdm_mic_init(uint32_t sample_rate);

// Decimate available PDM data into up to max_samples PCM samples.
// Returns the number of samples written. gain_q15 is a linear gain in Q15
// (32768 = unity) applied before saturation.
uint32_t pdm_mic_read(int16_t *out, uint32_t max_samples, int32_t gain_q15);

// Drop any buffered PDM data (e.g. when the host starts a stream).
void pdm_mic_flush(void);

#endif
