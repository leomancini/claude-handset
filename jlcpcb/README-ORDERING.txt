Claude Handset  #3cf75256 (patched)  -  JLCPCB order pack
=========================================================

Files in this folder
--------------------
claude-handset-gerbers-v3cf75256-patched.zip   upload this in "Add gerber file"
                                              (11 Gerber layers + Excellon drill, RS-274X)
claude-handset-BOM-JLCPCB.csv                  BOM in JLCPCB's format (Comment, Designator, Footprint, LCSC Part #)
claude-handset-CPL-JLCPCB.csv                  component placement list (Designator, Mid X, Mid Y, Layer, Rotation)

PCB order settings (the quote form defaults are NOT right for this board)
------------------------------------------------------------------------
Base material ............ FR-4
Layers ................... 4          <- the form defaults to 2; this board has two inner planes
Dimensions ............... 60 x 60 mm (round board, the outline file sets the shape)
PCB thickness ............ 1.6 mm
Surface finish ........... ENIG recommended (0.4 mm pitch QFN-56 and USON-8 flash); HASL works for a prototype
Outer / inner copper ..... 1 oz / 0.5 oz
Via covering ............. Tented (all vias are tented in the export)
Min via hole / diameter .. 0.3 mm / 0.5 mm are the smallest used (JLC 4-layer min is 0.15 / 0.25)
Impedance control ........ No
Remove order number ...... "Specify a location" is not set up; choose "No" or "Yes" (JLC picks a spot)
Confirm production file .. Yes (recommended, see "things to check" below)

Assembly (if ordering SMT)
--------------------------
Side ...................... Top only (all 51 parts are on the top side)
Every part carries an LCSC number. JLCPCB's BOM matcher also finds U5 (MCP1825T-3302E/DC) as
C622691 even though the BOM cell is blank; if it ever shows unmatched, use C622691 or the
pin-compatible 1 A sibling MCP1826T-3302E/DC (C512184). Do NOT accept MCP1825S / MCP1825ST
(3-pin versions): they have no SHDN/PWRGD pins.
J1 (USB4105-GF-A, C3020560) is flagged "difficult to process" and JLCPCB leaves its row
UNSELECTED (qty 0) after the BOM upload. Click its magnifier, search C3020560, press Select;
it then shows qty 5 and adds a $0.03/pc special-component fee. In stock (1158 on 3 Sep 2026).
J3 (JST PH right-angle header) is through-hole: JLCPCB quotes it as hand-soldered, or leave it off
and solder it yourself. All other parts are SMD on the top side.
Rotation: JLCPCB re-orients parts to their library convention during review; check the pin-1
orientation of U1, U2, U3, U5, D1 in the assembly preview before confirming.

Things to check in JLCPCB's Gerber viewer / DFM before confirming
-----------------------------------------------------------------
1. This export was hand-patched (see ../PATCH-NOTES.txt). Copper clearances around the new
   parts use negative-polarity Gerber objects (%LPC). Both my own checker and an independent
   parser (gerbonara) read them correctly; still, look at these four spots in the viewer and
   make sure the 3V3 pads/vias are NOT merged into the ground fill:
     - the via just below FB1 at board-centre coords (-12.8, -8.2)
     - the RP2040's north side, (-7 .. -2, 6 .. 8): C6, C11 and the moved 3V3 via
     - below the amp U3 at (11.5, -4.6): C24
     - the 3V3 via at (-1.4, 3.7) next to the RP2040's east pins
2. The original Flux layout has several spacings below JLCPCB's 0.09 mm minimum that were not
   touched: USB-C pads vs traces at 0.05 mm around (-0.3, -21.3), RUN pad vs SWDIO trace 0.07 mm
   at (-7.8, 5.4), F13/F14 traces vs 3V3 near the RP2040 east pins at 0.07-0.09 mm. Expect an
   engineering query; accept their "adjust spacing" fix if offered.
3. Silkscreen: the labels "C6" and "C11" still sit at the parts' old positions (cosmetic).

Cart as saved on 3 September 2026 (order 10211669A, not paid)
--------------------------------------------------------------
PCB   : 4 layers, 1.6 mm, ENIG, plugged vias, Confirm Production File = Yes ........ $28.38
PCBA  : Standard, top side, 5 pcs, 26 parts, Confirm Parts Placement = Yes ......... $154.40
U3 (MAX98357A) was rotated right 90 deg in the placement viewer so its pin-1 dot sits on the
footprint's pin-1 mark (top-left). All other parts were accepted as auto-aligned.
Note: the cart lists the PCB dimension as 70 x 70 mm. The outline is a 60 mm circle whose
Gerber coordinates span 10..70 mm, and JLCPCB measured from the origin. Production follows
the outline file; the size only affects price. Correct it to 60 x 60 in "Edit Order" if you
want the exact quote (last time Edit Order removed the item and re-saving failed, so only
do it if you are prepared to rebuild the cart).

Engineer query 4 September 2026 (order 10211669A): C25 pads too small for the part
------------------------------------------------------------------------------------
Correct. Flux's BOM put C25 on the 0805 line (C15850) but the copper footprint is 0603
(same pads as C1). Answer: option A, replace C25 with C96446 (CL10A106MA8NRNC, 10uF 25V
X5R 0603), the same part as C1. The BOM and CPL in this folder are now corrected, so a
future upload will not repeat it. All other passives were checked against their pads: OK.
Resolution 5 September 2026: JLCPCB's Replace Parts page groups C2, C25, C3 on one line, so
the whole line was switched to C96446 (0603) for this order. C2 and C3 sit on 0805 pads with
the smaller body; electrically identical. The BOM here keeps C2/C3 on C15850 (0805) and C25
on C96446 (0603), which is the correct split for any future order.

Production-file check 5 September 2026 (JLCPCB pack ~/Downloads/claude-handset-gerbers-v3cf75256-patched_Y8)
-----------------------------------------------------------------------------------------------------------
Their CAM output (ok/tl, l2, l3, bl, ts, bs, to, drl, ko) was compared against the uploaded files
(shift is exactly -5 mm, +75 mm; YG/ zip is byte-identical to the upload).
  copper top/bottom ... identical inside the board outline
  copper inner ........ identical except non-functional via/through-hole pads removed (standard CAM step)
  copper at edges ..... pulled back ~0.3 mm from the board edge and from the NPTH mic hole (standard)
  solder mask ......... identical, plus mask opened over the USB-C NPTH slots and the mic hole
  silkscreen .......... identical, clipped where it crossed pads
  drill ............... all 114 holes and 4 USB-C slots present; 4 extra tooling holes in the rails
  connectivity ........ netlist replayed on THEIR copper: 319 pads, 0 opens, 0 shorts
  panel ............... 70 x 70 mm frame: 5 mm rails left/right, 3 mm top/bottom, QR code + tooling
                        holes on the rails, via plugging list (sk). This is why the order says 70 x 70.
                        Boards ship in the frame (depanel = No); snap them out. Expect small flats/rough
                        spots at the 3 and 9 o'clock edges where the tabs were.
Verdict: OK to confirm.
