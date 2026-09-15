/*
 * PDM microphone: PIO capture, DMA ring, CIC + half-band decimation.
 */

#include "pdm_mic.h"

#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "pdm_mic.pio.h"

#ifndef PDM_SAMPLE_ON_HIGH_PHASE
#define PDM_SAMPLE_ON_HIGH_PHASE 0
#endif

//--------------------------------------------------------------------+
// Capture ring
//--------------------------------------------------------------------+

// 2048 words = 65536 PDM bits = 21 ms at 3.072 MHz.
#define PDM_RING_WORDS  2048u
#define PDM_RING_BYTES  (PDM_RING_WORDS * 4u)
#define PDM_RING_BITS   13   // log2(PDM_RING_BYTES)
#define PDM_WORD_MASK   (PDM_RING_WORDS - 1u)
#define PDM_BYTE_MASK   (PDM_RING_BYTES - 1u)

static uint32_t __attribute__((aligned(PDM_RING_BYTES))) pdm_ring[PDM_RING_WORDS];
static const uint8_t *const pdm_bytes = (const uint8_t *) pdm_ring;

static PIO  pdm_pio;
static uint pdm_sm;
static int  pdm_dma_a = -1;
static int  pdm_dma_b = -1;

// Consumer index (words).
static uint32_t pdm_rd;

//--------------------------------------------------------------------+
// Stage 1: sinc^4 decimate-by-32 via byte lookup tables
//--------------------------------------------------------------------+

#define CIC_R       32                      // decimation
#define CIC_TAPS    (4 * (CIC_R - 1) + 1)   // 125
#define CIC_BYTES   16                      // ceil(125 / 8)
#define CIC_SUM     (1u << 20)              // 32^4 = sum of the impulse response

static int32_t cic_lut[CIC_BYTES][256];

static void cic_build_tables(void) {
  // Impulse response of four cascaded 32-sample boxcars.
  static int32_t h[CIC_BYTES * 8];
  static int32_t tmp[CIC_BYTES * 8];
  memset(h, 0, sizeof(h));
  for (int i = 0; i < CIC_R; i++) h[i] = 1;
  int len = CIC_R;
  for (int stage = 1; stage < 4; stage++) {
    memset(tmp, 0, sizeof(tmp));
    for (int i = 0; i < len; i++)
      for (int j = 0; j < CIC_R; j++) tmp[i + j] += h[i];
    len += CIC_R - 1;
    memcpy(h, tmp, sizeof(h));
  }
  // len == CIC_TAPS; remaining entries up to CIC_BYTES * 8 stay zero.

  for (int b = 0; b < CIC_BYTES; b++) {
    for (int v = 0; v < 256; v++) {
      int32_t acc = 0;
      for (int k = 0; k < 8; k++) {
        if (v & (1 << k)) acc += h[8 * b + k];   // bit k = k-th sample in time
      }
      cic_lut[b][v] = acc;
    }
  }
}

// One 96 kHz output for the window starting at PDM word index w.
static inline int32_t cic_output(uint32_t w) {
  uint32_t base = w * 4u;
  int32_t acc = 0;
  for (int j = 0; j < CIC_BYTES; j++) {
    acc += cic_lut[j][pdm_bytes[(base + (uint32_t) j) & PDM_BYTE_MASK]];
  }
  // Bits are 0/1; map to -1/+1: 2 * acc - sum(h). Range +/- 2^20 -> 16-bit.
  return (2 * acc - (int32_t) CIC_SUM) >> 5;
}

//--------------------------------------------------------------------+
// Stage 2: DC blocker + half-band FIR decimate-by-2 (96 kHz -> 48 kHz)
//--------------------------------------------------------------------+

#define HB_TAPS     47
#define HB_CENTER   (HB_TAPS / 2)
#define HB_HIST     64
#define HB_HIST_MASK (HB_HIST - 1)

// Non-zero taps only: the centre tap and the odd offsets.
#define HB_NONZERO  (1 + 2 * ((HB_CENTER + 1) / 2))
static int32_t hb_coef[HB_NONZERO];
static uint8_t hb_delay[HB_NONZERO];

static int32_t hb_hist[HB_HIST];
static uint32_t hb_head;
static uint32_t hb_phase;

static int32_t dc_x_prev;
static int32_t dc_y_acc;     // y in Q8 so the leak term keeps its fraction

static void hb_build_taps(void) {
  // Windowed-sinc low-pass at fs/4 (Blackman window). Zeros land on the even
  // offsets automatically; keep the centre and the odd offsets.
  double coef[HB_TAPS];
  double sum = 0.0;
  for (int n = 0; n < HB_TAPS; n++) {
    int m = n - HB_CENTER;
    double h = (m == 0) ? 0.5 : sin(M_PI * m / 2.0) / (M_PI * m);
    double w = 0.42 - 0.5 * cos(2.0 * M_PI * n / (HB_TAPS - 1)) + 0.08 * cos(4.0 * M_PI * n / (HB_TAPS - 1));
    coef[n] = h * w;
    sum += coef[n];
  }
  int idx = 0;
  for (int n = 0; n < HB_TAPS; n++) {
    int m = n - HB_CENTER;
    if (m != 0 && (m & 1) == 0) continue;       // structural zero
    hb_coef[idx] = (int32_t) lrint(coef[n] / sum * 32768.0);
    hb_delay[idx] = (uint8_t) n;
    idx++;
  }
}

// Push one 96 kHz sample; returns true and sets *out every second sample.
static inline bool hb_push(int32_t x, int32_t *out) {
  hb_head = (hb_head + 1) & HB_HIST_MASK;
  hb_hist[hb_head] = x;
  hb_phase ^= 1;
  if (hb_phase) return false;

  int64_t acc = 0;
  for (int i = 0; i < HB_NONZERO; i++) {
    acc += (int64_t) hb_coef[i] * hb_hist[(hb_head - hb_delay[i]) & HB_HIST_MASK];
  }
  *out = (int32_t) (acc >> 15);
  return true;
}

static inline int32_t dc_block(int32_t x) {
  // y[n] = x[n] - x[n-1] + (1 - 1/256) y[n-1]   (~60 Hz corner at 96 kHz),
  // accumulated in Q8 so the leak does not truncate to a DC residual.
  dc_y_acc = ((x - dc_x_prev) << 8) + dc_y_acc - (dc_y_acc >> 8);
  dc_x_prev = x;
  return dc_y_acc >> 8;
}

//--------------------------------------------------------------------+
// DMA / PIO plumbing
//--------------------------------------------------------------------+

static void __isr pdm_dma_irq(void) {
  if (dma_channel_get_irq1_status(pdm_dma_a)) {
    dma_channel_acknowledge_irq1(pdm_dma_a);
    dma_channel_hw_addr(pdm_dma_a)->write_addr = (uintptr_t) pdm_ring;
  }
  if (dma_channel_get_irq1_status(pdm_dma_b)) {
    dma_channel_acknowledge_irq1(pdm_dma_b);
    dma_channel_hw_addr(pdm_dma_b)->write_addr = (uintptr_t) pdm_ring;
  }
}

static uint32_t pdm_write_index(void) {
  int ch = dma_channel_is_busy(pdm_dma_a) ? pdm_dma_a : pdm_dma_b;
  uintptr_t addr = dma_channel_hw_addr(ch)->write_addr;
  return ((addr - (uintptr_t) pdm_ring) / 4u) & PDM_WORD_MASK;
}

static void pdm_config_channel(int ch, int chain_to) {
  dma_channel_config c = dma_channel_get_default_config(ch);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, false);
  channel_config_set_write_increment(&c, true);
  channel_config_set_ring(&c, true, PDM_RING_BITS);
  channel_config_set_dreq(&c, pio_get_dreq(pdm_pio, pdm_sm, false));
  channel_config_set_chain_to(&c, chain_to);
  dma_channel_configure(ch, &c, pdm_ring, &pdm_pio->rxf[pdm_sm], PDM_RING_WORDS, false);
}

void pdm_mic_init(uint32_t sample_rate) {
  cic_build_tables();
  hb_build_taps();
  memset(pdm_ring, 0, sizeof(pdm_ring));

#if PDM_SAMPLE_ON_HIGH_PHASE
  const pio_program_t *program = &pdm_mic_alt_program;
#else
  const pio_program_t *program = &pdm_mic_program;
#endif

  uint offset;
  bool ok = pio_claim_free_sm_and_add_program_for_gpio_range(program, &pdm_pio, &pdm_sm, &offset,
                                                              HANDSET_PIN_PDM_DATA, 2, true);
  hard_assert(ok);
  pdm_mic_program_init(pdm_pio, pdm_sm, offset, HANDSET_PIN_PDM_DATA, HANDSET_PIN_PDM_CLK,
                       sample_rate * 64u, program);

  pdm_dma_a = dma_claim_unused_channel(true);
  pdm_dma_b = dma_claim_unused_channel(true);
  pdm_config_channel(pdm_dma_a, pdm_dma_b);
  pdm_config_channel(pdm_dma_b, pdm_dma_a);

  dma_channel_set_irq1_enabled(pdm_dma_a, true);
  dma_channel_set_irq1_enabled(pdm_dma_b, true);
  irq_add_shared_handler(DMA_IRQ_1, pdm_dma_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(DMA_IRQ_1, true);

  pdm_rd = 0;
  dma_channel_start(pdm_dma_a);
  pio_sm_set_enabled(pdm_pio, pdm_sm, true);
}

void pdm_mic_flush(void) {
  pdm_rd = pdm_write_index();
  hb_phase = 0;
}

uint32_t pdm_mic_read(int16_t *out, uint32_t max_samples, int32_t gain_q15) {
  uint32_t produced = 0;
  uint32_t wr = pdm_write_index();

  // Output n needs words n .. n+3 (125-bit window); keep a margin of a word
  // so we never read what DMA is still writing.
  while (produced < max_samples) {
    uint32_t avail = (wr - pdm_rd) & PDM_WORD_MASK;
    if (avail < CIC_BYTES / 4u + 2u) break;

    int32_t s96 = dc_block(cic_output(pdm_rd));
    pdm_rd = (pdm_rd + 1u) & PDM_WORD_MASK;

    int32_t s48;
    if (hb_push(s96, &s48)) {
      int32_t v = (int32_t) (((int64_t) s48 * gain_q15) >> 15);
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      out[produced++] = (int16_t) v;
    }
  }

  // If the consumer fell far behind (more than half the ring), skip ahead
  // rather than emit stale audio.
  uint32_t lag = (wr - pdm_rd) & PDM_WORD_MASK;
  if (lag > PDM_RING_WORDS / 2u) pdm_mic_flush();

  return produced;
}

void pdm_mic_debug(uint32_t *dma_write_addr, uint32_t *pio_pc) {
  int ch = dma_channel_is_busy(pdm_dma_a) ? pdm_dma_a : pdm_dma_b;
  *dma_write_addr = dma_channel_hw_addr(ch)->write_addr;
  *pio_pc = pio_sm_get_pc(pdm_pio, pdm_sm);
}
