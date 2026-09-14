# Wiring simulator

Pure-Python (no dependencies) simulator that tests the board *as built from the copper*, not as
drawn in the schematic.

```
python3 sim/extract_netlist.py     # copper -> asbuilt_netlist.json  (about 2 min)
python3 sim/simulator.py           # DC simulation + wiring tests   -> results.md
```

## What it does

1. **extract_netlist.py** parses the four copper Gerbers and the drill file, unions every trace,
   pad, via and fill that physically touches (honouring negative-polarity clears), and drops every
   IPC-D-356 netlist point onto the resulting copper islands. Each island becomes one as-built net.
   Orphan pads, shorted nets and split nets are reported.

2. **simulator.py** builds a DC circuit on those as-built nets:
   - resistor values from `pick_and_place.csv`; fuse, ferrite, ESD-array pass-through and the
     8 Ω speaker as small resistances; switches open/closed per scenario;
   - behavioural MCP1825 LDO (3.3 V, dropout, open-drain PWRGD), RP2040 core regulator, GPIO
     drivers and internal pull-ups, and the supply currents of every IC;
   - RP2040 pin numbers are recovered from pad geometry and cross-checked against the netlist
     labels, so tests can refer to GPIO numbers.

   It then runs:
   - **netlist equivalence**: copper vs intended nets, GPIO assignments from the project description;
   - **two-terminal wiring paths**: 51 resistance measurements through the passive network
     (USB D+/D- through the ESD array and 27 Ω, CC pull-downs, QSPI to the flash, I2S and
     SD_MODE to the amp, PDM to the mic, SWD header, crystal, speaker, pull-ups, switches, and
     "must be open" pairs such as 3V3-GND and D+ to D-);
   - **decoupling placement**: net membership and distance of the nearest capacitor to every
     supply pin (warnings only);
   - **DC scenarios**: USB idle, each button, reset, BOOTSEL, debugger RUN, amp playing at 1 W,
     amp off, USB brown-out (PWRGD must go low), USB unplugged. Node voltages, fuse current and
     LDO dissipation are checked against expected windows.

Run against a different export with `GERBER_ROOT=<dir> NETLIST_OUT=<file>` for the extractor and
`NETLIST_IN=<file> RESULTS_OUT=<file>` for the simulator. `results_original.md` is the same
suite run on the untouched Flux export, where it flags the short and the opens.

## Limits

DC only: capacitors and the crystal are open circuits, so oscillator start-up, USB signal
integrity and audio are not simulated. Load currents and the SD_MODE pull-down are datasheet
estimates (listed at the end of `results.md`).
