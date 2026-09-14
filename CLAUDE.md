# Claude Handset — project handoff

Read this first. It is the complete state of the hardware as of 14 September 2026, written for
the agent that writes the firmware. Everything below was verified from the manufacturing files,
not from the schematic, unless it says otherwise.

## What the device is

A 60 mm round, 1.6 mm, 4-layer PCB that plugs into a computer over USB-C and acts as a
**USB composite device: audio (speaker out + microphone in) plus a boot-compatible HID keyboard**
with two side buttons that send **F13** and **F14**. Designed in Flux.ai by Leo Mancini
(https://www.flux.ai/leomancini/claude-handset~2m, version #3cf75256). Intended to live in a
handset-shaped enclosure (see `mechanical/`) and be used as a push-to-talk voice handset for a
Claude session on the host computer: the host maps F13/F14 to actions, streams audio in and out.

The Flux project itself was never edited after export (the user ran out of Flux credits). All
fixes were made directly in the exported Gerbers. **Do not regenerate from Flux without re-applying
`PATCH-NOTES.txt`.**

## Hardware, as built

| Block | Part | LCSC | Notes |
|---|---|---|---|
| MCU | Raspberry Pi RP2040 (U1) | C2040 | QFN-56, 12 MHz crystal Y1 (ABM8-272-T3), 27 pF load caps |
| Flash | Winbond W25Q16JVUXIQ (U2) | C2843335 | 16 Mbit = **2 MB** QSPI, USON-8 |
| Speaker amp | MAX98357AETE+T (U3) | C910544 | Class-D I2S DAC, powered from **VBUS 5 V**, GAIN pin floating |
| Microphone | Knowles SPH0641LU4H-1 (MK1) | C2879853 | PDM MEMS, **bottom port** (hole through the PCB), SELECT tied to GND |
| 3.3 V LDO | Microchip MCP1825T-3302E/DC (U5) | C622691 | 500 mA, SHDN tied to VBUS (always on), PWRGD open-drain |
| USB-C | GCT USB4105-GF-A (J1) | C3020560 | USB 2.0, CC1/CC2 5.1 kΩ pull-downs (R1, R2) |
| USB protection | SMD050F-2 PTC fuse F1 (500 mA hold), SMF5.0A TVS D2, USBLC6-2SC6 ESD D1, 27 Ω series R3/R4 | | |
| Speaker connector | JST S2B-PH-K-S (J3), 2-pin 2.0 mm right-angle | C173752 | SPKR_P / SPKR_N differential; use a 4–8 Ω speaker |
| Debug header | Samtec FTSH-103-01-L-DV (J4), 2×3 1.27 mm | C3324375 | pins: 1=3V3 2=SWDIO 3=GND 4=SWCLK 5=RUN 6=NC |
| Side buttons | Omron B3U-3000P (SW1 F13, SW2 F14), side-actuated | C963349 | active-low, 10 kΩ pull-ups R14/R15 |
| Tactile buttons | TS-1088 (SW3 = RESET on RUN, SW4 = BOOTSEL on flash CS) | C720477 | on the top face |

### RP2040 pin map (from the netlist, confirmed against the copper)

| GPIO | Net | Function | Direction / notes |
|---|---|---|---|
| GPIO2 | I2S_BCLK | MAX98357A BCLK | out |
| GPIO3 | I2S_LRCLK | MAX98357A LRCLK (word select) | out |
| GPIO4 | I2S_DAC_DATA | MAX98357A DIN | out |
| GPIO5 | PDM_DATA | SPH0641 DATA | in |
| GPIO6 | PDM_CLK | SPH0641 CLOCK | out, 1.0–3.25 MHz normal mode (datasheet), e.g. 3.072 MHz for 48 kHz × 64 |
| GPIO7 | F13_BTN_N | SW1 (side button 1) | in, **active low**, external 10 kΩ pull-up to 3V3 |
| GPIO8 | F14_BTN_N | SW2 (side button 2) | in, **active low**, external 10 kΩ pull-up to 3V3 |
| GPIO9 | 3V3_PWRGD | LDO power-good | in, open-drain with 10 kΩ pull-up (R9): high = 3V3 OK, low = brown-out |
| GPIO21 | AMP_SD_MODE | MAX98357A SD_MODE | out, **high = amp on**, low = shutdown (see amp notes) |
| GPIO0, 1, 10–20, 22–29 | — | not connected | free for firmware use as internal-only signals (no pads) |
| XIN/XOUT | XOSC | 12 MHz crystal, 1 kΩ series on XOUT (R5) | standard Pico values |
| RUN | MCU_RUN | SW3 to GND, 10 kΩ pull-up (R8), also on J4 pin 5 | |
| QSPI_SS | QSPI_SS_MCU → 1 kΩ (R6) → QSPI_CS_FLASH | SW4 pulls the flash CS low = **BOOTSEL** (hold SW4, press/release SW3) | 10 kΩ pull-up R7 |
| USB_DP/USB_DM | via 27 Ω R3/R4 and ESD array D1 to J1 | | |
| SWCLK/SWDIO | J4 pins 4 / 2 | | |

Nothing is on the ADC pins; ADC_AVDD is simply tied to 3V3. There is no LED, no battery, no
charger, no external RTC. IOVDD = 3V3 everywhere, DVDD = RP2040 internal 1.1 V regulator.

### Power

USB VBUS → F1 (PTC) → **VBUS_5V** → D2 (TVS) and D1. VBUS_5V feeds the amp directly and the LDO.
LDO output **3V3** feeds the RP2040, flash, and (through ferrite FB1) the mic on **MIC_3V3_FILT**.
Bus-powered only; the whole board budget must stay under 500 mA (fuse) and USB enumeration rules.
The amp can draw peaks of several hundred mA at full volume into 4 Ω; a 3.2 W amp on a 500 mA
budget means firmware should keep digital volume moderate (start at −12 dB or lower and tune).

### Amp notes (MAX98357A)

- I2S, 16/24/32-bit, LRCLK 8–96 kHz. BCLK must be 32 × or 64 × LRCLK (it locks to the ratio).
- **SD_MODE voltage selects the channel**: driven high at 3.3 V (> 1.4 V) the amp plays the
  **left** channel only. So send mono audio in the left slot, or duplicate it into both.
- GAIN floating = **9 dB** gain (datasheet default). Cannot be changed in firmware.
- SD_MODE low = shutdown, ~µA. Use it to mute between playback to avoid idle hiss and save power.
  Ramp-up after enabling takes ~1 ms; the amp auto-mutes on no-clock.

### Microphone notes (SPH0641LU4H-1)

- PDM, single-bit. SELECT is tied to GND, so the mic drives data on one clock phase; the other
  phase is high-Z (only one mic on the bus, so treat it as mono). Check the datasheet timing
  table for which edge to sample; the RP2040 PIO PDM examples (e.g. `pico-pdm-microphone`)
  handle this.
- Bottom-ported: the sound hole is the 0.5 mm NPTH at board (−19, −11). The enclosure must leave
  an acoustic path under the board there.
- Power comes through a 600 Ω ferrite, so keep the mic's current transients small (it draws ~250 µA).

### Buttons

SW1 (F13) and SW2 (F14) are the user buttons, side-actuated, on the board edge at roughly
1 o'clock and 3 o'clock. Debounce in firmware (~10 ms). Because the pull-ups are external, do not
enable the internal pull-downs. SW3 is a hard reset and SW4 is BOOTSEL; neither is a user input.

## Suggested firmware architecture

- **SDK**: pico-sdk (C/C++) with TinyUSB, or CircuitPython/MicroPython. TinyUSB has a UAC2 audio
  class example (`examples/device/uac2_headset`) that is almost exactly this device: speaker +
  mic composite, add a HID keyboard interface for F13/F14 (HID usage IDs 0x68 and 0x69).
- **Audio out**: I2S via PIO (`pico-extras` `audio_i2s`), DMA-fed from the USB OUT endpoint.
  Set GPIO21 high before starting the clocks.
- **Audio in**: PDM via PIO + DMA, CIC/FIR decimate to 16 kHz or 48 kHz PCM, feed the USB IN endpoint.
- **Boot-compatible HID**: keyboard boot protocol so the buttons work in BIOS/recovery screens too.
- **PWRGD on GPIO9**: optionally poll it; if it goes low, mute the amp.
- **Flash**: 2 MB total, so keep the image well under that; no filesystem needed.
- Flashing: hold SW4 (BOOTSEL), tap SW3 (RESET), release SW4 → RPI-RP2 drive appears, copy the
  UF2. Or use the SWD header J4 with a Debug Probe (pinout above; it is NOT the Pico 3-pin order).

## Manufacturing status

- **Order**: JLCPCB order **10211669A** (PCB Y8-10211669A + Standard PCBA SMT026090460419),
  placed 5 September 2026, 5 boards, fully assembled top side including the through-hole speaker
  header. ENIG, 4-layer, plugged vias, Confirm Production File and Confirm Parts Placement both
  used and both confirmed. Total about $180 plus shipping.
- One engineer query was raised and resolved: C25's footprint is 0603 but the BOM had it on the
  0805 line. The whole C2/C25/C3 line was switched to C96446 (0603 10 µF). The BOM files in
  `jlcpcb/` are corrected for future orders. Details in `jlcpcb/README-ORDERING.txt`.
- Boards ship in a 70 × 70 mm assembly frame with rails; snap them out. Expect small rough spots
  at the 3 and 9 o'clock edges.
- The uploaded files are exactly the Gerbers in this repo root. JLCPCB's reprocessed production
  files were diffed against them layer by layer and the netlist was replayed on their copper:
  identical inside the outline, 0 opens, 0 shorts.

## What was fixed in the Gerbers (summary of `PATCH-NOTES.txt`)

Flux's export had a 3V3-to-PWRGD short through a misplaced via, an open in the 3V3 net feeding
the RP2040's regulator and USB supply pins, and no ground connection on the RP2040 exposed pad or
on C10/C13/C14/C16. Also 34 parts had no manufacturer part numbers. All were fixed by editing the
Gerbers, drill and IPC-D-356 netlist directly, with negative-polarity clearances in the ground
planes. Three decoupling caps (C6, C11, C24) were moved next to the pins they serve. A pure-Python
connectivity checker, an independent parser (gerbonara), JLCPCB's DFM tool and JLCPCB's own CAM
output all agree the result is clean. Pre-existing sub-0.09 mm spacings (USB-C pads, RUN pad vs
SWDIO) were left alone and JLCPCB did not object.

## Repository map

```
claude-handset-*.gbr, .drl, .d356   patched manufacturing files (the truth)
pick_and_place.csv                  component positions, board centre origin, mm
BOM/                                Flux BOM exports, JLCPCB one corrected
jlcpcb/                             order pack: zip that was uploaded, BOM/CPL, README-ORDERING.txt
                                    (settings, engineer queries, production-file verification)
PATCH-NOTES.txt                     every edit made to the Gerbers, with coordinates
patch-renders/                      before/after images of each repair site
original-flux-export/               untouched Flux export for reference
sim/                                netlist extractor + DC simulator, results.md (142 pass / 0 fail)
mechanical/                         STLs for the enclosure designer (board, assembly, keep-outs)
readme.txt                          Flux's original index file
```

Coordinate convention everywhere in this repo: **mm, X right, Y up, origin at the board centre**.
The Gerbers themselves put the centre at (40, −40); JLCPCB's CAM shifted that to (35, 35).

## Tooling notes for the next agent

- `python3 sim/extract_netlist.py && python3 sim/simulator.py` re-verifies the wiring from copper
  (about 2 min, no dependencies).
- `python3 mechanical/make_stl.py` regenerates the STLs from `pick_and_place.csv`.
- gerbonara (`pip install gerbonara`) plus `rsvg-convert` render any layer to PNG for inspection.
- There is no firmware directory yet. Put it in `firmware/` at the root.
