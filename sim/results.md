# Wiring simulation results

Board: Claude Handset #3cf75256, netlist from asbuilt_netlist.json

**142 checks passed, 0 failed, 1 warnings (placement, not wiring).**


## Netlist: as-built copper vs intended

| Check | Result | Detail |
|---|---|---|
| no orphan pads / shorts / split nets | PASS | all pads on the intended nets |
| RP2040 pin numbering matches netlist labels | PASS | 56 pins identified from pad geometry |
| GPIO2 = I2S_BCLK (per project description) | PASS | GPIO2 is on I2S_BCLK |
| GPIO3 = I2S_LRCLK (per project description) | PASS | GPIO3 is on I2S_LRCLK |
| GPIO4 = I2S_DAC_DATA (per project description) | PASS | GPIO4 is on I2S_DAC_DATA |
| GPIO5 = PDM_DATA (per project description) | PASS | GPIO5 is on PDM_DATA |
| GPIO6 = PDM_CLK (per project description) | PASS | GPIO6 is on PDM_CLK |
| GPIO21 = AMP_SD_MODE (per project description) | PASS | GPIO21 is on AMP_SD_MODE |
| GPIO7 assignment | PASS | GPIO7 is on F13_BTN_N |
| GPIO8 assignment | PASS | GPIO8 is on F14_BTN_N |
| GPIO9 assignment | PASS | GPIO9 is on 3V3_PWRGD |
| all RP2040 IOVDD pins on one net | PASS | IOVDD pins: {'3V3'} |
| all RP2040 DVDD pins on one net | PASS | DVDD pins: {'1V1_CORE'} |
| RP2040 exposed pad on GND | PASS | EP is on GND |
| TESTEN tied to GND | PASS | TESTEN on GND |
| no unpowered IC supply pins | PASS | U2/U3/MK1/U5 supply pins connected |

## Wiring paths (2-terminal resistance, ICs unpowered)

| Check | Result | Detail |
|---|---|---|
| USB D+ : J1.DP1 -> D1 -> R3 -> RP2040 USB_DP | PASS | measured 27 Ω, expected 27 Ω |
| USB D- : J1.DN1 -> D1 -> R4 -> RP2040 USB_DM | PASS | measured 27 Ω, expected 27 Ω |
| USB D+ both connector pins tied (DP1-DP2) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| USB D- both connector pins tied (DN1-DN2) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| CC1 pull-down to GND (R1) | PASS | measured 5.1e+03 Ω, expected 5.1e+03 Ω |
| CC2 pull-down to GND (R2) | PASS | measured 5.1e+03 Ω, expected 5.1e+03 Ω |
| VBUS: J1.VBUS -> F1 -> VBUS_5V (fuse) | PASS | measured 0.5 Ω, expected 0.5 Ω |
| VBUS_5V -> amp VDD | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| VBUS_5V -> LDO SHDN (always enabled) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| 3V3 -> FB1 -> mic VDD | PASS | measured 0.8 Ω, expected 0.8 Ω |
| 3V3 -> flash VCC | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| 3V3 -> SWD header pin 1 | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| RP2040 VREG_VOUT -> DVDD (1V1 core) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| XIN -> Y1 | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| XOUT -> R5 -> Y1 | PASS | measured 1e+03 Ω, expected 1e+03 Ω |
| QSPI_SS -> R6 -> flash CS | PASS | measured 1e+03 Ω, expected 1e+03 Ω |
| flash CS pull-up R7 to 3V3 | PASS | measured 1e+04 Ω, expected 1e+04 Ω |
| QSPI_SCLK -> flash CLK | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| QSPI_SD0 -> flash DI (IO0) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| QSPI_SD1 -> flash DO (IO1) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| QSPI_SD2 -> flash WP# (IO2) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| QSPI_SD3 -> flash HOLD (IO3) | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| GPIO2 -> amp BCLK | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| GPIO3 -> amp LRCLK | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| GPIO4 -> amp DIN | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| GPIO21 -> amp SD_MODE | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| amp OUTP -> J3 pin 1 | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| amp OUTN -> J3 pin 2 | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| speaker terminals not grounded (J3.1 -> GND) | PASS | measured open (>1 MΩ), expected open |
| speaker terminals not grounded (J3.2 -> GND) | PASS | measured open (>1 MΩ), expected open |
| GPIO6 -> mic CLK | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| GPIO5 -> mic DATA | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| mic SELECT tied low | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| SWD header: SWDIO | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| SWD header: SWCLK | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| SWD header: RUN | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| SWD header: GND | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| RUN pull-up R8 to 3V3 | PASS | measured 1e+04 Ω, expected 1e+04 Ω |
| PWRGD pull-up R9 to 3V3 | PASS | measured 1e+04 Ω, expected 1e+04 Ω |
| PWRGD -> GPIO9 | PASS | measured 0.0 mΩ, expected 0.0 mΩ |
| F13 pull-up R14 to 3V3 | PASS | measured 1e+04 Ω, expected 1e+04 Ω |
| F14 pull-up R15 to 3V3 | PASS | measured 1e+04 Ω, expected 1e+04 Ω |
| no DC path 3V3 -> GND with everything off | PASS | measured open (>1 MΩ), expected open |
| no DC path VBUS_5V -> GND with everything off | PASS | measured open (>1 MΩ), expected open |
| no DC path 1V1 -> GND with everything off | PASS | measured open (>1 MΩ), expected open |
| USB D+ isolated from D- | PASS | measured open (>1 MΩ), expected open |
| USB D+ isolated from GND | PASS | measured open (>1 MΩ), expected open |
| SW1 shorts F13_BTN_N to GND | PASS | closed: 50.0 mΩ; open: open (>1 MΩ) |
| SW2 shorts F14_BTN_N to GND | PASS | closed: 50.0 mΩ; open: open (>1 MΩ) |
| SW3 shorts MCU_RUN to GND | PASS | closed: 50.0 mΩ; open: open (>1 MΩ) |
| SW4 shorts QSPI_CS_FLASH to GND | PASS | closed: 50.0 mΩ; open: open (>1 MΩ) |

## Decoupling capacitors (net + distance to the pin they serve)

| Check | Result | Detail |
|---|---|---|
| RP2040 pin 1 IOVDD (3V3) | PASS | nearest cap C5 100 nF at 2.07 mm |
| RP2040 pin 10 IOVDD (3V3) | PASS | nearest cap C4 100 nF at 2.20 mm |
| RP2040 pin 22 IOVDD (3V3) | WARN | nearest cap C6 100 nF at 3.45 mm |
| RP2040 pin 33 IOVDD (3V3) | PASS | nearest cap C7 100 nF at 2.20 mm |
| RP2040 pin 42 IOVDD (3V3) | PASS | nearest cap C8 100 nF at 2.07 mm |
| RP2040 pin 49 IOVDD (3V3) | PASS | nearest cap C15 100 nF at 2.68 mm |
| RP2040 pin 48 USB_VDD (3V3) | PASS | nearest cap C15 100 nF at 3.00 mm |
| RP2040 pin 43 ADC_AVDD (3V3) | PASS | nearest cap C13 1000 nF at 2.12 mm |
| RP2040 pin 44 VREG_VIN (3V3) | PASS | nearest cap C13 1000 nF at 2.36 mm |
| RP2040 pin 23 DVDD (1V1_CORE) | PASS | nearest cap C11 100 nF at 1.07 mm |
| RP2040 pin 50 DVDD (1V1_CORE) | PASS | nearest cap C14 1000 nF at 2.12 mm |
| RP2040 pin 45 VREG_VOUT (1V1_CORE) | PASS | nearest cap C14 1000 nF at 1.95 mm |
| flash VCC (3V3) | PASS | nearest cap C17 100 nF at 1.49 mm |
| amp VDD (VBUS_5V) | PASS | nearest cap C24 100 nF at 1.18 mm |
| mic VDD (filtered) (MIC_3V3_FILT) | PASS | nearest cap C28 100 nF at 1.65 mm |
| LDO input (VBUS_5V) | PASS | nearest cap C2 10000 nF at 2.50 mm |
| LDO output (3V3) | PASS | nearest cap C3 10000 nF at 2.34 mm |
| crystal load caps 27 pF on XIN and XOUT sides | PASS | C12 on {'GND', 'XOSC_XI'}, C18 on {'GND', 'XOSC_XO'} |
| RUN reset RC (R8 + C20) | PASS | C20 on {'GND', 'MCU_RUN'}; tau = 1.0 ms |

## DC scenarios

| Check | Result | Detail |
|---|---|---|
| USB plugged, idle: V(USB_VBUS_RAW) | PASS | 4.994 V (expected 4.95..5.0 V) |
| USB plugged, idle: V(VBUS_5V) | PASS | 4.967 V (expected 4.9..5.0 V) |
| USB plugged, idle: V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| USB plugged, idle: V(1V1_CORE) | PASS | 1.096 V (expected 1.05..1.15 V) |
| USB plugged, idle: V(MIC_3V3_FILT) | PASS | 3.297 V (expected 3.25..3.35 V) |
| USB plugged, idle: V(3V3_PWRGD) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(MCU_RUN) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(QSPI_CS_FLASH) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(QSPI_SS_MCU) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(F13_BTN_N) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(F14_BTN_N) | PASS | 3.297 V (expected 3.2..3.35 V) |
| USB plugged, idle: V(AMP_SD_MODE) | PASS | 0.000 V (expected 0..0.05 V) |
| USB plugged, idle: V(USB_CC1) | PASS | 0.000 V (expected 0..0.01 V) |
| USB plugged, idle: V(USB_CC2) | PASS | 0.000 V (expected 0..0.01 V) |
| USB plugged, idle: fuse current below 0.5 A hold | PASS | 55 mA through F1 |
| USB plugged, idle: LDO within 500 mA and dissipation sane | PASS | LDO out 55.2 mA, dissipation 92 mW |
| F13 button pressed: V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| F13 button pressed: V(F13_BTN_N) | PASS | 0.000 V (expected 0..0.05 V) |
| F13 button pressed: V(F14_BTN_N) | PASS | 3.297 V (expected 3.2..3.35 V) |
| F13 button pressed: fuse current below 0.5 A hold | PASS | 56 mA through F1 |
| F13 button pressed: LDO within 500 mA and dissipation sane | PASS | LDO out 55.5 mA, dissipation 93 mW |
| F14 button pressed: V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| F14 button pressed: V(F13_BTN_N) | PASS | 3.297 V (expected 3.2..3.35 V) |
| F14 button pressed: V(F14_BTN_N) | PASS | 0.000 V (expected 0..0.05 V) |
| F14 button pressed: fuse current below 0.5 A hold | PASS | 56 mA through F1 |
| F14 button pressed: LDO within 500 mA and dissipation sane | PASS | LDO out 55.5 mA, dissipation 93 mW |
| SW3 pressed (RUN low = reset): V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| SW3 pressed (RUN low = reset): V(MCU_RUN) | PASS | 0.000 V (expected 0..0.05 V) |
| SW3 pressed (RUN low = reset): V(QSPI_CS_FLASH) | PASS | 3.297 V (expected 3.2..3.35 V) |
| SW3 pressed (RUN low = reset): fuse current below 0.5 A hold | PASS | 56 mA through F1 |
| SW3 pressed (RUN low = reset): LDO within 500 mA and dissipation sane | PASS | LDO out 55.5 mA, dissipation 93 mW |
| SW4 pressed (flash CS low = BOOTSEL): V(MCU_RUN) | PASS | 3.297 V (expected 3.2..3.35 V) |
| SW4 pressed (flash CS low = BOOTSEL): V(QSPI_CS_FLASH) | PASS | 0.000 V (expected 0..0.05 V) |
| SW4 pressed (flash CS low = BOOTSEL): V(QSPI_SS_MCU) | PASS | 0.058 V (expected 0..0.4 V) |
| SW4 pressed (flash CS low = BOOTSEL): fuse current below 0.5 A hold | PASS | 56 mA through F1 |
| SW4 pressed (flash CS low = BOOTSEL): LDO within 500 mA and dissipation sane | PASS | LDO out 55.6 mA, dissipation 93 mW |
| Debugger holds RUN low via J4: V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| Debugger holds RUN low via J4: V(MCU_RUN) | PASS | 0.000 V (expected 0..0.05 V) |
| Debugger holds RUN low via J4: fuse current below 0.5 A hold | PASS | 56 mA through F1 |
| Debugger holds RUN low via J4: LDO within 500 mA and dissipation sane | PASS | LDO out 55.5 mA, dissipation 93 mW |
| Amp enabled, 1 W playing (GPIO21 high): V(VBUS_5V) | PASS | 4.828 V (expected 4.7..5.0 V) |
| Amp enabled, 1 W playing (GPIO21 high): V(3V3) | PASS | 3.297 V (expected 3.25..3.35 V) |
| Amp enabled, 1 W playing (GPIO21 high): V(AMP_SD_MODE) | PASS | 3.296 V (expected 3.0..3.35 V) |
| Amp enabled, 1 W playing (GPIO21 high): fuse current below 0.5 A hold | PASS | 287 mA through F1 |
| Amp enabled, 1 W playing (GPIO21 high): LDO within 500 mA and dissipation sane | PASS | LDO out 55.2 mA, dissipation 84 mW |
| Amp disabled (GPIO21 low): V(AMP_SD_MODE) | PASS | 0.000 V (expected 0..0.05 V) |
| Amp disabled (GPIO21 low): fuse current below 0.5 A hold | PASS | 55 mA through F1 |
| Amp disabled (GPIO21 low): LDO within 500 mA and dissipation sane | PASS | LDO out 55.2 mA, dissipation 92 mW |
| USB brown-out to 3.0 V: V(3V3) | PASS | 2.865 V (expected 2.7..3.0 V) |
| USB brown-out to 3.0 V: V(3V3_PWRGD) | PASS | 0.017 V (expected 0..0.2 V) |
| USB brown-out to 3.0 V: fuse current below 0.5 A hold | PASS | 54 mA through F1 |
| USB brown-out to 3.0 V: LDO within 500 mA and dissipation sane | PASS | LDO out 53.6 mA, dissipation 6 mW |
| USB unplugged: V(VBUS_5V) | PASS | 0.000 V (expected 0..0.01 V) |
| USB unplugged: V(3V3) | PASS | 0.000 V (expected 0..0.01 V) |
| USB unplugged: V(1V1_CORE) | PASS | 0.000 V (expected 0..0.01 V) |
| USB unplugged: V(3V3_PWRGD) | PASS | 0.000 V (expected 0..0.01 V) |
| USB unplugged: V(MCU_RUN) | PASS | 0.000 V (expected 0..0.01 V) |

## Scenario operating points

### USB plugged, idle

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 | 4.95..5.0 | ok |
| VBUS_5V | 4.967 | 4.9..5.0 | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 | 1.05..1.15 | ok |
| MIC_3V3_FILT | 3.297 | 3.25..3.35 | ok |
| 3V3_PWRGD | 3.297 | 3.2..3.35 | ok |
| MCU_RUN | 3.297 | 3.2..3.35 | ok |
| QSPI_CS_FLASH | 3.297 | 3.2..3.35 | ok |
| QSPI_SS_MCU | 3.297 | 3.2..3.35 | ok |
| F13_BTN_N | 3.297 | 3.2..3.35 | ok |
| F14_BTN_N | 3.297 | 3.2..3.35 | ok |
| AMP_SD_MODE | 0.000 | 0..0.05 | ok |
| USB_CC1 | 0.000 | 0..0.01 | ok |
| USB_CC2 | 0.000 | 0..0.01 | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.4 mA, LDO out 55.2 mA (92 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### F13 button pressed

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 3.297 |  | ok |
| QSPI_CS_FLASH | 3.297 |  | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 0.000 | 0..0.05 | ok |
| F14_BTN_N | 3.297 | 3.2..3.35 | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.7 mA, LDO out 55.5 mA (93 mW), RP2040 core reg 39.9 mA, I(R14) -330 µA, I(R7) -0 µA, I(R9) -0 µA

### F14 button pressed

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 3.297 |  | ok |
| QSPI_CS_FLASH | 3.297 |  | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 3.297 | 3.2..3.35 | ok |
| F14_BTN_N | 0.000 | 0..0.05 | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.7 mA, LDO out 55.5 mA (93 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### SW3 pressed (RUN low = reset)

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 0.000 | 0..0.05 | ok |
| QSPI_CS_FLASH | 3.297 | 3.2..3.35 | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 3.297 |  | ok |
| F14_BTN_N | 3.297 |  | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.7 mA, LDO out 55.5 mA (93 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### SW4 pressed (flash CS low = BOOTSEL)

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 |  | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 3.297 | 3.2..3.35 | ok |
| QSPI_CS_FLASH | 0.000 | 0..0.05 | ok |
| QSPI_SS_MCU | 0.058 | 0..0.4 | ok |
| F13_BTN_N | 3.297 |  | ok |
| F14_BTN_N | 3.297 |  | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.8 mA, LDO out 55.6 mA (93 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -330 µA, I(R9) -0 µA

### Debugger holds RUN low via J4

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 0.000 | 0..0.05 | ok |
| QSPI_CS_FLASH | 3.297 |  | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 3.297 |  | ok |
| F14_BTN_N | 3.297 |  | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.7 mA, LDO out 55.5 mA (93 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### Amp enabled, 1 W playing (GPIO21 high)

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.971 |  | ok |
| VBUS_5V | 4.828 | 4.7..5.0 | ok |
| 3V3 | 3.297 | 3.25..3.35 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 3.297 |  | ok |
| QSPI_CS_FLASH | 3.297 |  | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 3.297 |  | ok |
| F14_BTN_N | 3.297 |  | ok |
| AMP_SD_MODE | 3.296 | 3.0..3.35 | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 287.0 mA, LDO out 55.2 mA (84 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### Amp disabled (GPIO21 low)

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 4.994 |  | ok |
| VBUS_5V | 4.967 |  | ok |
| 3V3 | 3.297 |  | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 3.297 |  | ok |
| 3V3_PWRGD | 3.297 |  | ok |
| MCU_RUN | 3.297 |  | ok |
| QSPI_CS_FLASH | 3.297 |  | ok |
| QSPI_SS_MCU | 3.297 |  | ok |
| F13_BTN_N | 3.297 |  | ok |
| F14_BTN_N | 3.297 |  | ok |
| AMP_SD_MODE | 0.000 | 0..0.05 | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 55.4 mA, LDO out 55.2 mA (92 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -0 µA

### USB brown-out to 3.0 V

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 2.995 |  | ok |
| VBUS_5V | 2.968 |  | ok |
| 3V3 | 2.865 | 2.7..3.0 | ok |
| 1V1_CORE | 1.096 |  | ok |
| MIC_3V3_FILT | 2.864 |  | ok |
| 3V3_PWRGD | 0.017 | 0..0.2 | ok |
| MCU_RUN | 2.865 |  | ok |
| QSPI_CS_FLASH | 2.865 |  | ok |
| QSPI_SS_MCU | 2.865 |  | ok |
| F13_BTN_N | 2.865 |  | ok |
| F14_BTN_N | 2.865 |  | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 53.8 mA, LDO out 53.6 mA (6 mW), RP2040 core reg 39.9 mA, I(R14) -0 µA, I(R7) -0 µA, I(R9) -285 µA

### USB unplugged

| Net | V | expected | ok |
|---|---|---|---|
| USB_VBUS_RAW | 0.000 |  | ok |
| VBUS_5V | 0.000 | 0..0.01 | ok |
| 3V3 | 0.000 | 0..0.01 | ok |
| 1V1_CORE | 0.000 | 0..0.01 | ok |
| MIC_3V3_FILT | 0.000 |  | ok |
| 3V3_PWRGD | 0.000 | 0..0.01 | ok |
| MCU_RUN | 0.000 | 0..0.01 | ok |
| QSPI_CS_FLASH | 0.000 |  | ok |
| QSPI_SS_MCU | 0.000 |  | ok |
| F13_BTN_N | 0.000 |  | ok |
| F14_BTN_N | 0.000 |  | ok |
| AMP_SD_MODE | 0.000 |  | ok |
| USB_CC1 | 0.000 |  | ok |
| USB_CC2 | 0.000 |  | ok |
| SPKR_P | 0.000 |  | ok |
| SPKR_N | 0.000 |  | ok |

fuse 0.0 mA, LDO out 0.0 mA (0 mW), RP2040 core reg 0.0 mA, I(R14) 0 µA, I(R7) 0 µA, I(R9) 0 µA


## Model assumptions

- Sources/loads: USB 5 V with 0.1 Ω; fuse 0.5 Ω; ferrite 0.8 Ω; ESD array pass-through 10 mΩ; speaker 8 Ω.
- MCP1825: 3.3 V out, 100 mV dropout, PWRGD open-drain pulls low below 92 % of 3.3 V, enabled when SHDN > 1.5 V.
- RP2040: core regulator 1.1 V when VREG_VIN > 1.6 V; core 40 mA, IOVDD 5 mA, USB_VDD 5 mA, ADC 0.5 mA; QSPI_SS/SWD internal 56 kΩ pull-ups; GPIO drivers 30 Ω.
- Flash 3 mA, mic 0.7 mA, amp 0.1 mA off / 240 mA at 1 W into 8 Ω; MAX98357A SD_MODE assumed to have a 100 kΩ internal pull-down.
- Capacitors and the crystal are open at DC; they are checked by net membership, value and distance instead.
- Two-terminal resistances are measured with all ICs unpowered and switches open unless stated.