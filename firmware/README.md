# Claude Handset firmware

RP2040 firmware for the handset PCB. One USB full-speed composite device:

| Interface | What it is |
|---|---|
| UAC2 audio, speaker | 48 kHz, 16-bit stereo OUT, asynchronous with an explicit feedback endpoint. Mixed to mono, host volume + mute applied in firmware, played through the MAX98357A over I2S. The amp's SD_MODE pin is only high while the host is streaming and not muted. |
| UAC2 audio, microphone | 48 kHz, 16-bit mono IN. The Knowles PDM mic is clocked at 3.072 MHz by PIO and decimated in firmware (sinc⁴ ÷32 → DC blocker → half-band FIR ÷2). Host "input volume" is a digital gain, 0 to +30 dB, default +12 dB. |
| HID keyboard | Boot-protocol keyboard. SW1 sends **F17**, SW2 sends **F18** (see *Keys* below). 10 ms debounce, remote wakeup. |
| Reset | Raspberry Pi vendor "reset" interface so `picotool` can reboot the board into BOOTSEL without touching the buttons. |

macOS enumerates it as "Claude Handset" with 2 output and 1 input channel; the buttons show up as a keyboard.

## Build

Needs cmake, ninja, an `arm-none-eabi` GCC and the Pico SDK 2.3.1 (with its TinyUSB submodule). `build.sh` looks for the SDK in `~/.pico-sdk/sdk/2.3.1` and the toolchain in `~/.pico-sdk/toolchain/*/bin`, or set `PICO_SDK_PATH` / `PICO_TOOLCHAIN_PATH`.

```sh
brew install cmake ninja picotool
mkdir -p ~/.pico-sdk/sdk && git clone --depth 1 --branch 2.3.1 https://github.com/raspberrypi/pico-sdk ~/.pico-sdk/sdk/2.3.1
git -C ~/.pico-sdk/sdk/2.3.1 submodule update --init --depth 1 lib/tinyusb
# ARM toolchain: unpack arm-gnu-toolchain-*-darwin-arm64-arm-none-eabi.tar.xz into ~/.pico-sdk/toolchain/

./build.sh                        # -> build/claude_handset/claude_handset.uf2
./build.sh claude_handset_kb2040  # bench build for an Adafruit KB2040 wired to the same GPIOs
```

## Flash

```sh
./flash.sh          # picotool load -f --vid 0x1209 --pid 0x0001 -x build/claude_handset/claude_handset.uf2
```

Works in three states: blank flash (board boots straight into the ROM bootloader), a board put into BOOTSEL by hand (hold SW4, tap SW3, release SW4), or this firmware running (the reset interface reboots it). picotool only looks for the reset interface on Raspberry Pi's VID unless `--vid/--pid` are given, hence the flags.

## Recovery and crash diagnostics

The main loop feeds a 2 s watchdog. If it ever stalls:

1. First watchdog reboot: the board comes back normally but its USB serial number gets a suffix, `<uid>-WDT<stage>-<pc>-<loops>` (hex). `stage` is the last breadcrumb (`DIAG_CRUMB` in the sources; 10–15 = main-loop tasks, 20–24 speaker path, 3x set-interface, 4x control requests). Bit 0x8000 set = hard fault and `pc` is the faulting address; bit 0x4000 = `panic()` and `pc` is the address of the format string, `loops` the caller. Map addresses with `arm-none-eabi-addr2line -e build/claude_handset/claude_handset.elf`.
2. Second watchdog reboot in a row (within 10 s of boot): the board drops into the ROM bootloader and shows up as RPI-RP2, so it can always be reflashed.

Read the serial with `ioreg -p IOUSB -l -w0 | grep kUSBSerialNumberString`.

## Keys

The board was designed around F13/F14, but macOS binds F14/F15 to display brightness on keyboards without dedicated keys, so the firmware sends F17 and F18, which have no default binding on macOS, Windows or Linux. Change `HANDSET_KEY_SW1` / `HANDSET_KEY_SW2` in `src/buttons.c` to move them. `tools/keytest.html` is a browser page that lights up when the keys arrive.

## Tunables

All in `src/usb_audio.c`:

- `SPK_HEADROOM_DB` (−6 dB): fixed attenuation on top of the host volume. The amp has 9 dB of gain and the board must stay under the 500 mA fuse, so a full-scale signal is never sent. Raise with a current meter in hand.
- `SPK_VOL_*_DB`, `MIC_VOL_*_DB`: ranges and defaults advertised to the host.
- `SPK_I2S_TARGET_FRAMES` (192 = 4 ms): how far ahead of the DMA reader the I2S ring is kept. Total speaker latency is this plus ~4 ms of USB FIFO.

`-DPDM_SAMPLE_ON_HIGH_PHASE=1` selects the alternate PDM sample edge (see `src/pdm_mic.pio`).

## Layout

```
boards/           board headers (claude_handset.h: pin map, 2 MB flash, QSPI /4)
src/main.c        init order + cooperative main loop
src/usb_descriptors.[ch]  device/config/string descriptors, UAC2 topology, HID report
src/usb_audio.[ch]        UAC2 requests, stream on/off, speaker + mic tasks, amp enable,
                          RP2040 ISO endpoint workaround
src/i2s_out.[ch] + .pio   I2S TX: PIO + two chained DMA channels on a 4 KB ring
src/pdm_mic.[ch] + .pio   PDM RX: PIO + DMA ring, CIC/half-band decimator
src/buttons.[ch]          debounce + HID keyboard reports
src/usb_reset.c           picotool reset interface (TinyUSB app class driver)
src/diag.[ch]             watchdog policy, breadcrumbs, panic/hard-fault capture
src/tusb_config.h         TinyUSB configuration
tools/keytest.html        browser key tester
```

## Notes on things that bit

- **TinyUSB RP2040 ISO endpoints**: `usbd_edpt_close()` is a no-op on this port and re-activating an ISO endpoint does not clear its hardware buffer control. When the host stops a stream while a buffer is armed and later restarts it, the driver panics with "ep was already available". `rp2040_abort_endpoint()` in `usb_audio.c` aborts and clears the endpoints whenever a streaming interface goes to alt 0.
- **PDM sample point**: sample the data line on the clock edge that ends the mic's driving phase, with the PIO input synchroniser bypassed. Sampling near the other edge lands in the mic's output transition and shows up as noise.
- **DC blocker**: a first-order integer DC blocker leaves a residual of up to 255 counts unless the leak term keeps fractional bits; it is accumulated in Q8.
- **macOS makes a new USB audio device the default input and output.** Handy for testing, surprising for system sounds. `SwitchAudioSource` or the Sound settings put it back.
