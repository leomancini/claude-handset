/*
 * USB audio glue.
 *
 * Speaker: the TinyUSB OUT FIFO is regulated to half full by the feedback
 * endpoint (AUDIO_FEEDBACK_METHOD_FIFO_COUNT). We drain it at exactly the I2S
 * rate, mix stereo down to mono (the MAX98357A plays a single channel), apply
 * the host's volume, and keep the I2S DMA ring topped up ~4 ms ahead. When the
 * stream stops or underruns we go back to "priming" and emit silence until the
 * FIFO has refilled to its regulation point.
 *
 * Microphone: whatever the PDM decimator has produced is pushed into the IN
 * FIFO; TinyUSB's flow control trims packets by a sample either way so the
 * asynchronous source stays in step with the host.
 */

#include "usb_audio.h"

#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/structs/usb.h"
#include "hardware/structs/usb_dpram.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "i2s_out.h"
#include "pdm_mic.h"
#include "diag.h"

//--------------------------------------------------------------------+
// Tunables
//--------------------------------------------------------------------+

// Fixed attenuation applied on top of the host volume. The amp has 9 dB of
// gain and the whole board must stay under 500 mA, so a full-scale signal is
// never sent to it. Adjust with care and a current meter.
#define SPK_HEADROOM_DB        -6.0f

// Host-visible volume range, in dB.
#define SPK_VOL_MIN_DB         -60
#define SPK_VOL_MAX_DB         0
#define SPK_VOL_DEFAULT_DB     -12

// Microphone digital gain range. SPH0641 sensitivity is -26 dBFS at 94 dB SPL,
// so close talking sits around -30 dBFS before gain.
#define MIC_VOL_MIN_DB         0
#define MIC_VOL_MAX_DB         30
#define MIC_VOL_DEFAULT_DB     12

// Frames kept queued in the I2S ring ahead of the DMA reader.
#define SPK_I2S_TARGET_FRAMES  192u   // 4 ms

// FIFO level (bytes) at which we start draining after a (re)start.
#define SPK_PRIME_BYTES        (CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 2)

//--------------------------------------------------------------------+
// State
//--------------------------------------------------------------------+

static bool    spk_streaming;    // speaker interface at alt 1
static bool    mic_streaming;    // microphone interface at alt 1
static bool    spk_primed;       // FIFO reached its regulation point since start
static bool    power_good = true;

static int8_t  spk_mute;
static int16_t spk_volume;       // UAC2 units: 1/256 dB
static int8_t  mic_mute;
static int16_t mic_volume;

static int32_t spk_gain_q15;
static int32_t mic_gain_q15;

static uint32_t const sample_rate = HANDSET_SAMPLE_RATE;

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+

static int32_t db_to_q15(float db) {
  float g = powf(10.0f, db / 20.0f) * 32768.0f;
  if (g > 0x7FFFFFFF / 2) g = 0x7FFFFFFF / 2;
  return (int32_t) (g + 0.5f);
}

static void update_gains(void) {
  spk_gain_q15 = spk_mute ? 0 : db_to_q15((float) spk_volume / 256.0f + SPK_HEADROOM_DB);
  mic_gain_q15 = mic_mute ? 0 : db_to_q15((float) mic_volume / 256.0f);
}

static void update_amp(void) {
  bool on = spk_streaming && !spk_mute && power_good;
  gpio_put(HANDSET_PIN_AMP_SD, on);
}

void usb_audio_set_power_good(bool ok) {
  if (ok != power_good) {
    power_good = ok;
    update_amp();
  }
}

//--------------------------------------------------------------------+
// Init
//--------------------------------------------------------------------+

void usb_audio_init(void) {
  gpio_init(HANDSET_PIN_AMP_SD);
  gpio_put(HANDSET_PIN_AMP_SD, 0);
  gpio_set_dir(HANDSET_PIN_AMP_SD, GPIO_OUT);

  spk_mute = 0;
  spk_volume = SPK_VOL_DEFAULT_DB * 256;
  mic_mute = 0;
  mic_volume = MIC_VOL_DEFAULT_DB * 256;
  update_gains();

  i2s_out_init(sample_rate);
  pdm_mic_init(sample_rate);
}

//--------------------------------------------------------------------+
// Speaker task
//--------------------------------------------------------------------+

void usb_audio_speaker_task(void) {
  uint32_t queued = i2s_out_queued();
  if (queued >= SPK_I2S_TARGET_FRAMES) return;
  uint32_t need = SPK_I2S_TARGET_FRAMES - queued;

  if (!spk_streaming) {
    i2s_out_write_silence(need);
    return;
  }

  DIAG_CRUMB(20);
  uint16_t avail = tud_audio_available();
  if (!spk_primed) {
    if (avail < SPK_PRIME_BYTES) {
      i2s_out_write_silence(need);
      return;
    }
    spk_primed = true;
  }

  uint32_t have = avail / 4u;   // stereo 16-bit frames
  if (have == 0) {
    // Underrun: back to priming so we do not chase the host packet by packet.
    spk_primed = false;
    i2s_out_write_silence(need);
    return;
  }

  static uint32_t buf[64];
  while (need > 0 && have > 0) {
    DIAG_CRUMB(21);
    uint32_t n = need < 64u ? need : 64u;
    if (n > have) n = have;
    uint16_t got = tud_audio_read(buf, (uint16_t) (n * 4u));
    DIAG_CRUMB(22);
    n = got / 4u;
    if (n == 0) break;

    for (uint32_t i = 0; i < n; i++) {
      int16_t l = (int16_t) (buf[i] & 0xFFFFu);
      int16_t r = (int16_t) (buf[i] >> 16);
      int32_t m = ((int32_t) l + (int32_t) r) >> 1;
      m = (int32_t) (((int64_t) m * spk_gain_q15) >> 15);
      if (m > 32767) m = 32767;
      if (m < -32768) m = -32768;
      uint32_t s = (uint32_t) m & 0xFFFFu;
      buf[i] = s | (s << 16);   // same sample in both slots
    }
    DIAG_CRUMB(23);
    i2s_out_write(buf, n);
    need -= n;
    have -= n;
  }
  DIAG_CRUMB(24);
}

//--------------------------------------------------------------------+
// Microphone task
//--------------------------------------------------------------------+

void usb_audio_mic_task(void) {
  static int16_t buf[96];
  uint32_t n = pdm_mic_read(buf, sizeof(buf) / sizeof(buf[0]), mic_gain_q15);
  if (n == 0) return;
  if (!mic_streaming) return;   // decimator keeps running so the filters stay warm
  tud_audio_write(buf, (uint16_t) (n * sizeof(int16_t)));
}

//--------------------------------------------------------------------+
// RP2040 workaround: TinyUSB's usbd_edpt_close() is a no-op for isochronous
// endpoints on this port, and re-activating one only re-enables it. If the
// host stops a stream (alt 0) while a buffer is still armed, the next start
// (alt 1) re-arms it and the driver panics with "ep was already available".
// So when a streaming interface closes, abort the endpoint in hardware and
// clear its buffer control ourselves.
//--------------------------------------------------------------------+

#define usb_hw_set   ((usb_hw_t *) hw_set_alias_untyped(usb_hw))
#define usb_hw_clear ((usb_hw_t *) hw_clear_alias_untyped(usb_hw))

static void rp2040_abort_endpoint(uint8_t ep_addr) {
  uint num = ep_addr & 0x0Fu;
  bool in = (ep_addr & 0x80u) != 0;
  uint32_t mask = 1u << (num * 2u + (in ? 0u : 1u));   // same layout as buf_status / abort

  // EP_ABORT exists on B2 and later silicon (the handset uses B2).
  if (rp2040_chip_version() >= 2) {
    usb_hw_set->abort = mask;
    uint32_t deadline = time_us_32() + 2000;
    while ((usb_hw->abort_done & mask) != mask && (int32_t) (time_us_32() - deadline) < 0) {
      tight_loop_contents();
    }
  }
  if (in) {
    usb_dpram->ep_buf_ctrl[num].in = 0;
  } else {
    usb_dpram->ep_buf_ctrl[num].out = 0;
  }
  usb_hw_clear->buf_status = mask;
  if (rp2040_chip_version() >= 2) {
    usb_hw_clear->abort_done = mask;
    usb_hw_clear->abort = mask;
  }
}

//--------------------------------------------------------------------+
// Streaming interface callbacks
//--------------------------------------------------------------------+

static void set_interface(uint8_t itf, uint8_t alt) {
  DIAG_CRUMB(30 + itf * 2 + (alt ? 1 : 0));
  if (alt == 0) {
    if (itf == ITF_NUM_AUDIO_STREAMING_SPK) {
      rp2040_abort_endpoint(EPNUM_AUDIO_OUT);
      rp2040_abort_endpoint(EPNUM_AUDIO_FB);
    } else if (itf == ITF_NUM_AUDIO_STREAMING_MIC) {
      rp2040_abort_endpoint(EPNUM_AUDIO_IN);
    }
  }
  if (itf == ITF_NUM_AUDIO_STREAMING_SPK) {
    spk_streaming = (alt != 0);
    spk_primed = false;
    i2s_out_reset();
    tud_audio_clear_ep_out_ff();
    update_amp();
  } else if (itf == ITF_NUM_AUDIO_STREAMING_MIC) {
    mic_streaming = (alt != 0);
    tud_audio_clear_ep_in_ff();
    pdm_mic_flush();
  }
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  set_interface(tu_u16_low(tu_le16toh(p_request->wIndex)), tu_u16_low(tu_le16toh(p_request->wValue)));
  return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  set_interface(tu_u16_low(tu_le16toh(p_request->wIndex)), 0);
  return true;
}

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t *feedback_param) {
  (void) func_id; (void) alt_itf;
  feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
  feedback_param->sample_freq = sample_rate;
}

//--------------------------------------------------------------------+
// Control requests
//--------------------------------------------------------------------+

static bool clock_get_request(uint8_t rhport, audio_control_request_t const *request) {
  if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
    if (request->bRequest == AUDIO_CS_REQ_CUR) {
      audio_control_cur_4_t cur = { .bCur = (int32_t) tu_htole32(sample_rate) };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &cur, sizeof(cur));
    }
    if (request->bRequest == AUDIO_CS_REQ_RANGE) {
      audio_control_range_4_n_t(1) range = {
        .wNumSubRanges = tu_htole16(1),
        .subrange[0] = { .bMin = (int32_t) sample_rate, .bMax = (int32_t) sample_rate, .bRes = 0 },
      };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &range, sizeof(range));
    }
  } else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR) {
    audio_control_cur_1_t valid = { .bCur = 1 };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &valid, sizeof(valid));
  }
  return false;
}

static bool clock_set_request(audio_control_request_t const *request, uint8_t const *buf) {
  TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);
  if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
    TU_VERIFY(request->wLength == sizeof(audio_control_cur_4_t));
    // Only one rate is offered; accept it and ignore anything else.
    return true;
  }
  return false;
}

static bool feature_unit_get_request(uint8_t rhport, audio_control_request_t const *request,
                                     int8_t mute, int16_t volume, int16_t vmin_db, int16_t vmax_db) {
  if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
    audio_control_cur_1_t cur = { .bCur = mute };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &cur, sizeof(cur));
  }
  if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
    if (request->bRequest == AUDIO_CS_REQ_RANGE) {
      audio_control_range_2_n_t(1) range = {
        .wNumSubRanges = tu_htole16(1),
        .subrange[0] = { .bMin = tu_htole16(vmin_db * 256), .bMax = tu_htole16(vmax_db * 256), .bRes = tu_htole16(256) },
      };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &range, sizeof(range));
    }
    if (request->bRequest == AUDIO_CS_REQ_CUR) {
      audio_control_cur_2_t cur = { .bCur = tu_htole16(volume) };
      return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &cur, sizeof(cur));
    }
  }
  return false;
}

static bool feature_unit_set_request(audio_control_request_t const *request, uint8_t const *buf,
                                     int8_t *mute, int16_t *volume, int16_t vmin_db, int16_t vmax_db) {
  TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);
  if (request->bControlSelector == AUDIO_FU_CTRL_MUTE) {
    TU_VERIFY(request->wLength == sizeof(audio_control_cur_1_t));
    *mute = ((audio_control_cur_1_t const *) buf)->bCur;
    return true;
  }
  if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
    TU_VERIFY(request->wLength == sizeof(audio_control_cur_2_t));
    int16_t v = (int16_t) tu_le16toh((uint16_t) ((audio_control_cur_2_t const *) buf)->bCur);
    if (v < vmin_db * 256) v = vmin_db * 256;
    if (v > vmax_db * 256) v = vmax_db * 256;
    *volume = v;
    return true;
  }
  return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  DIAG_CRUMB(40);
  audio_control_request_t const *request = (audio_control_request_t const *) p_request;
  switch (request->bEntityID) {
    case UAC2_ENTITY_CLOCK:
      return clock_get_request(rhport, request);
    case UAC2_ENTITY_SPK_FEATURE_UNIT:
      return feature_unit_get_request(rhport, request, spk_mute, spk_volume, SPK_VOL_MIN_DB, SPK_VOL_MAX_DB);
    case UAC2_ENTITY_MIC_FEATURE_UNIT:
      return feature_unit_get_request(rhport, request, mic_mute, mic_volume, MIC_VOL_MIN_DB, MIC_VOL_MAX_DB);
    default:
      return false;
  }
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
  (void) rhport;
  DIAG_CRUMB(41);
  audio_control_request_t const *request = (audio_control_request_t const *) p_request;
  bool ok = false;
  switch (request->bEntityID) {
    case UAC2_ENTITY_CLOCK:
      ok = clock_set_request(request, buf);
      break;
    case UAC2_ENTITY_SPK_FEATURE_UNIT:
      ok = feature_unit_set_request(request, buf, &spk_mute, &spk_volume, SPK_VOL_MIN_DB, SPK_VOL_MAX_DB);
      break;
    case UAC2_ENTITY_MIC_FEATURE_UNIT:
      ok = feature_unit_set_request(request, buf, &mic_mute, &mic_volume, MIC_VOL_MIN_DB, MIC_VOL_MAX_DB);
      break;
    default:
      break;
  }
  if (ok) {
    update_gains();
    update_amp();
  }
  return ok;
}
