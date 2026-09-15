/*
 * Read the live status block from a running Claude Handset over its vendor
 * interface. Build: cc -O -o handset-status handset-status.c -I/opt/homebrew/include -L/opt/homebrew/lib -lusb-1.0   (brew install libusb)
 * Usage: handset-status [interval_seconds]   (repeats when an interval is given)
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

#include "../src/handset_status.h"

#define VID 0x1209
#define PID 0x0001
#define RESET_ITF 4

static void print_status(const handset_status_t *s) {
  printf("uptime %u.%03u s  loops %u\n", s->uptime_ms / 1000, s->uptime_ms % 1000, s->loops);
  printf("speaker: streaming=%u primed=%u mute=%u volume=%.2f dB gain_q15=%d  amp_gpio=%u power_good=%u\n",
         s->spk_streaming, s->spk_primed, s->spk_mute, s->spk_volume / 256.0, s->spk_gain_q15, s->amp_gpio, s->power_good);
  printf("         usb_out_fifo=%u B  i2s_queued=%u frames  usb_frames=%u silence_frames=%u underruns=%u\n",
         s->usb_out_fifo, s->i2s_queued, s->spk_usb_frames, s->spk_silence_frames, s->spk_underruns);
  printf("mic:     streaming=%u mute=%u volume=%.2f dB gain_q15=%d usb_in_fifo=%u B samples=%u\n",
         s->mic_streaming, s->mic_mute, s->mic_volume / 256.0, s->mic_gain_q15, s->usb_in_fifo, s->mic_samples);
  printf("i2s: dma_read=0x%08x pio_pc=%u   pdm: dma_write=0x%08x pio_pc=%u\n",
         s->i2s_dma_read_addr, s->pio_i2s_pc, s->pdm_dma_write_addr, s->pio_pdm_pc);
}

int main(int argc, char **argv) {
  int interval = argc > 1 ? atoi(argv[1]) : 0;

  libusb_context *ctx;
  if (libusb_init(&ctx) != 0) { fprintf(stderr, "libusb init failed\n"); return 1; }
  libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (!h) { fprintf(stderr, "no handset found (VID %04x PID %04x)\n", VID, PID); return 1; }
  int rc = libusb_claim_interface(h, RESET_ITF);
  if (rc != 0) fprintf(stderr, "warning: claim interface %d: %s\n", RESET_ITF, libusb_error_name(rc));

  do {
    handset_status_t st;
    memset(&st, 0, sizeof(st));
    rc = libusb_control_transfer(h,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        HANDSET_REQUEST_STATUS, 0, RESET_ITF, (unsigned char *) &st, sizeof(st), 1000);
    if (rc < 0) { fprintf(stderr, "control transfer failed: %s\n", libusb_error_name(rc)); return 1; }
    if (rc < (int) sizeof(st) || st.magic != HANDSET_STATUS_MAGIC) {
      fprintf(stderr, "short/invalid reply (%d bytes, magic %08x)\n", rc, st.magic); return 1;
    }
    print_status(&st);
    if (interval) { printf("\n"); fflush(stdout); sleep(interval); }
  } while (interval);

  libusb_release_interface(h, RESET_ITF);
  libusb_close(h);
  libusb_exit(ctx);
  return 0;
}
