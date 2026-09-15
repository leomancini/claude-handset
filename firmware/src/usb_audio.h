/*
 * USB audio glue: UAC2 control requests, speaker and microphone streaming,
 * amplifier enable.
 */

#ifndef USB_AUDIO_H_
#define USB_AUDIO_H_

#include <stdbool.h>

void usb_audio_init(void);

// Main-loop tasks
void usb_audio_speaker_task(void);
void usb_audio_mic_task(void);

// Tell the audio layer whether the 3.3 V rail is good. When false the amp is
// held in shutdown.
void usb_audio_set_power_good(bool ok);

#endif
