/*
 * I2S output: PIO + two chained DMA channels reading a 4 KB aligned ring.
 *
 * Both DMA channels are configured identically (ring wrap on the read address,
 * one full ring per transfer) and chain to each other, so playback never stops
 * and needs no CPU involvement. The completion IRQ only re-arms each channel's
 * read address at the ring base as belt-and-braces against wrap edge cases.
 */

#include "i2s_out.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "i2s_out.pio.h"

#define I2S_RING_BYTES  (I2S_RING_FRAMES * 4u)
#define I2S_RING_BITS   12   // log2(I2S_RING_BYTES)

static uint32_t __attribute__((aligned(I2S_RING_BYTES))) i2s_ring[I2S_RING_FRAMES];

static PIO  i2s_pio;
static uint i2s_sm;
static int  i2s_dma_a = -1;
static int  i2s_dma_b = -1;

// Producer index (frames, modulo ring size).
static uint32_t i2s_wr;

static void __isr i2s_dma_irq(void) {
  if (dma_channel_get_irq0_status(i2s_dma_a)) {
    dma_channel_acknowledge_irq0(i2s_dma_a);
    dma_channel_hw_addr(i2s_dma_a)->read_addr = (uintptr_t) i2s_ring;
  }
  if (dma_channel_get_irq0_status(i2s_dma_b)) {
    dma_channel_acknowledge_irq0(i2s_dma_b);
    dma_channel_hw_addr(i2s_dma_b)->read_addr = (uintptr_t) i2s_ring;
  }
}

static uint32_t i2s_read_index(void) {
  int ch = dma_channel_is_busy(i2s_dma_a) ? i2s_dma_a : i2s_dma_b;
  uintptr_t addr = dma_channel_hw_addr(ch)->read_addr;
  return ((addr - (uintptr_t) i2s_ring) / 4u) & I2S_RING_MASK;
}

static void i2s_config_channel(int ch, int chain_to) {
  dma_channel_config c = dma_channel_get_default_config(ch);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  channel_config_set_ring(&c, false, I2S_RING_BITS);
  channel_config_set_dreq(&c, pio_get_dreq(i2s_pio, i2s_sm, true));
  channel_config_set_chain_to(&c, chain_to);
  dma_channel_configure(ch, &c, &i2s_pio->txf[i2s_sm], i2s_ring, I2S_RING_FRAMES, false);
}

void i2s_out_init(uint32_t sample_rate) {
  memset(i2s_ring, 0, sizeof(i2s_ring));

  uint offset;
  bool ok = pio_claim_free_sm_and_add_program_for_gpio_range(&i2s_out_program, &i2s_pio, &i2s_sm, &offset,
                                                              HANDSET_PIN_I2S_BCLK, 3, true);
  hard_assert(ok);
  i2s_out_program_init(i2s_pio, i2s_sm, offset, HANDSET_PIN_I2S_DATA, HANDSET_PIN_I2S_BCLK, sample_rate);

  i2s_dma_a = dma_claim_unused_channel(true);
  i2s_dma_b = dma_claim_unused_channel(true);
  i2s_config_channel(i2s_dma_a, i2s_dma_b);
  i2s_config_channel(i2s_dma_b, i2s_dma_a);

  dma_channel_set_irq0_enabled(i2s_dma_a, true);
  dma_channel_set_irq0_enabled(i2s_dma_b, true);
  irq_add_shared_handler(DMA_IRQ_0, i2s_dma_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(DMA_IRQ_0, true);

  i2s_wr = 0;
  dma_channel_start(i2s_dma_a);
  pio_sm_set_enabled(i2s_pio, i2s_sm, true);
  i2s_out_reset();
}

uint32_t i2s_out_queued(void) {
  return (i2s_wr - i2s_read_index()) & I2S_RING_MASK;
}

uint32_t i2s_out_free(void) {
  // Leave a small guard so the writer never lands on the word being read.
  uint32_t queued = i2s_out_queued();
  uint32_t limit = I2S_RING_FRAMES - 8u;
  return queued < limit ? limit - queued : 0;
}

uint32_t i2s_out_write(const uint32_t *frames, uint32_t count) {
  uint32_t n = i2s_out_free();
  if (count < n) n = count;
  for (uint32_t i = 0; i < n; i++) {
    i2s_ring[i2s_wr] = frames[i];
    i2s_wr = (i2s_wr + 1u) & I2S_RING_MASK;
  }
  return n;
}

uint32_t i2s_out_write_silence(uint32_t count) {
  uint32_t n = i2s_out_free();
  if (count < n) n = count;
  for (uint32_t i = 0; i < n; i++) {
    i2s_ring[i2s_wr] = 0;
    i2s_wr = (i2s_wr + 1u) & I2S_RING_MASK;
  }
  return n;
}

void i2s_out_reset(void) {
  // Restart just ahead of the reader: whatever it is about to read is
  // overwritten with silence.
  memset(i2s_ring, 0, sizeof(i2s_ring));
  i2s_wr = (i2s_read_index() + 4u) & I2S_RING_MASK;
}
