#!/usr/bin/env python3
"""Step 2 of the wiring simulator: DC circuit simulation of the AS-BUILT board.

The circuit is assembled from asbuilt_netlist.json (copper-derived nets), part values from
pick_and_place.csv, and behavioural models of the ICs (LDO, RP2040 core regulator, open-drain
power-good, GPIO drivers, loads).  A modified-nodal-analysis DC solver (pure Python) is run for a
set of scenarios - USB idle, each button, BOOTSEL, reset, brownout, amp playing, USB unplugged -
and node voltages / branch currents are checked against expectations.  Two-terminal resistance
measurements verify every signal path (USB data through the ESD array and series resistors,
QSPI, I2S, PDM, SWD, crystal, speaker).  Results go to results.md and stdout.
"""
import os, sys, csv, json, math, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GND = "GND"

# ----------------------------------------------------------------------------- inputs
NL = json.load(open(os.environ.get("NETLIST_IN", os.path.join(HERE, "asbuilt_netlist.json"))))
PADS = NL["pads"]

def parse_value(s):
    s = s.replace("Ω", "").replace("F", "").strip()
    m = re.match(r"^([\d.]+)([pnumkMG]?)$", s)
    if not m: return None
    return float(m.group(1)) * {"": 1, "p": 1e-12, "n": 1e-9, "u": 1e-6, "m": 1e-3, "k": 1e3, "M": 1e6, "G": 1e9}[m.group(2)]

VALUES = {}
with open(os.path.join(ROOT, "pick_and_place.csv"), encoding="utf-8-sig") as f:
    for row in csv.DictReader(f):
        v = parse_value(row["Value"]) if row["Value"] else None
        if v is not None: VALUES[row["Designator"]] = v

def pads_of(ref):
    return [p for p in PADS if p["ref"] == ref]

def net(ref, pin, nth=0):
    """As-built net of a pad. For U1, pin is the RP2040 signal name (GPIO7, IOVDD, ...)."""
    if ref == "U1":
        return U1[pin][nth]["asbuilt"] if isinstance(U1[pin], list) else U1[pin]["asbuilt"]
    c = [p for p in pads_of(ref) if p["pin"] == pin]
    return c[nth]["asbuilt"]

# ------------------------------------------------------------- RP2040 pin identification
RP2040_NAMES = ["IOVDD", "GPIO0", "GPIO1", "GPIO2", "GPIO3", "GPIO4", "GPIO5", "GPIO6", "GPIO7", "IOVDD",
                "GPIO8", "GPIO9", "GPIO10", "GPIO11", "GPIO12", "GPIO13", "GPIO14", "GPIO15", "TESTEN", "XIN",
                "XOUT", "IOVDD", "DVDD", "SWCLK", "SWD", "RUN", "GPIO16", "GPIO17", "GPIO18", "GPIO19",
                "GPIO20", "GPIO21", "IOVDD", "GPIO22", "GPIO23", "GPIO24", "GPIO25", "GPIO26", "GPIO27", "GPIO28",
                "GPIO29", "IOVDD", "ADC_AVDD", "VREG_VIN", "VREG_VOUT", "USB_DM", "USB_DP", "USB_VDD", "IOVDD", "DVDD",
                "QSPI_SD3", "QSPI_SCLK", "QSPI_SD0", "QSPI_SD2", "QSPI_SD1", "QSPI_SS"]
# what the IPC-356 4-character pin label must start with, for the pins that have a unique label
RP2040_LABEL = {"IOVDD": "IOVD", "TESTEN": "TEST", "XIN": "XIN", "XOUT": "XOUT", "DVDD": "DVDD", "SWCLK": "SWCL",
                "SWD": "SWD", "RUN": "RUN", "ADC_AVDD": "ADC", "VREG_VIN": "VREG", "VREG_VOUT": "VREG",
                "USB_DM": "USB", "USB_DP": "USB", "USB_VDD": "USB", "QSPI_SD3": "QSPI", "QSPI_SCLK": "QSPI",
                "QSPI_SD0": "QSPI", "QSPI_SD2": "QSPI", "QSPI_SD1": "QSPI", "QSPI_SS": "QSPI"}

def identify_rp2040():
    """Assign QFN-56 pin numbers from pad geometry (U1 centre (-6,2), rotated 180 deg):
    east row south->north = pins 1-14, north row east->west = 15-28, west row north->south = 29-42,
    south row west->east = 43-56."""
    u1 = [p for p in pads_of("U1") if p["pin"] != "GND"]
    cx, cy = -6.0, 2.0
    east = sorted([p for p in u1 if p["x"] > cx + 3], key=lambda p: p["y"])
    north = sorted([p for p in u1 if p["y"] > cy + 3], key=lambda p: -p["x"])
    west = sorted([p for p in u1 if p["x"] < cx - 3], key=lambda p: -p["y"])
    south = sorted([p for p in u1 if p["y"] < cy - 3], key=lambda p: p["x"])
    order = east + north + west + south
    assert len(order) == 56, len(order)
    pins = collections.defaultdict(list); mism = []
    for n, p in enumerate(order, 1):
        name = RP2040_NAMES[n-1]; p["rp_pin"] = n; p["rp_name"] = name
        pins[name].append(p)
        lab = RP2040_LABEL.get(name, "GPIO")
        if not p["pin"].startswith(lab): mism.append(f"pin {n} expected {name} ({lab}) but netlist label is {p['pin']}")
    ep = [p for p in pads_of("U1") if p["pin"] == "GND"]
    pins["GND"] = ep
    out = {k: (v if len(v) > 1 else v[0]) for k, v in pins.items()}
    return out, mism

U1, U1_MISMATCH = identify_rp2040()

# ------------------------------------------------------------------ DC solver (nodal)
class Circuit:
    def __init__(self):
        self.R = []; self.I = []; self.V = []; self.names = set()
    def add_R(self, a, b, r, tag=""):
        if a == b: return
        self.R.append((a, b, r, tag)); self.names |= {a, b}
    def add_I(self, frm, to, i, tag=""):        # current i flows out of `frm` into `to` (a load at frm when to=GND)
        self.I.append((frm, to, i, tag)); self.names |= {frm, to}
    def add_V(self, p, n, v, rs=0.01, tag=""):  # voltage source with series resistance rs
        self.V.append((p, n, v, rs, tag)); self.names |= {p, n}
    def solve(self):
        nodes = sorted(n for n in self.names if n != GND)
        idx = {n: i for i, n in enumerate(nodes)}; N = len(nodes)
        G = [[0.0]*N for _ in range(N)]; rhs = [0.0]*N
        def stamp_g(a, b, g):
            if a != GND: G[idx[a]][idx[a]] += g
            if b != GND: G[idx[b]][idx[b]] += g
            if a != GND and b != GND: G[idx[a]][idx[b]] -= g; G[idx[b]][idx[a]] -= g
        for n in nodes: G[idx[n]][idx[n]] += 1e-9          # 1 GOhm leak keeps floating nodes defined
        for a, b, r, _ in self.R: stamp_g(a, b, 1.0/r)
        for frm, to, i, _ in self.I:
            if frm != GND: rhs[idx[frm]] -= i
            if to != GND: rhs[idx[to]] += i
        for p, n, v, rs, _ in self.V:                     # Norton equivalent
            stamp_g(p, n, 1.0/rs)
            if p != GND: rhs[idx[p]] += v/rs
            if n != GND: rhs[idx[n]] -= v/rs
        # gaussian elimination with partial pivoting
        A = [G[i][:] + [rhs[i]] for i in range(N)]
        for c in range(N):
            piv = max(range(c, N), key=lambda r: abs(A[r][c]))
            A[c], A[piv] = A[piv], A[c]
            if abs(A[c][c]) < 1e-15: continue
            for r in range(c+1, N):
                f = A[r][c]/A[c][c]
                if f:
                    for k in range(c, N+1): A[r][k] -= f*A[c][k]
        x = [0.0]*N
        for r in range(N-1, -1, -1):
            s = A[r][N] - sum(A[r][k]*x[k] for k in range(r+1, N))
            x[r] = s/A[r][r] if abs(A[r][r]) > 1e-15 else 0.0
        v = {n: x[idx[n]] for n in nodes}; v[GND] = 0.0
        return v
    def branch_currents(self, v):
        out = {}
        for a, b, r, tag in self.R:
            if tag: out[tag] = (v[a]-v[b])/r
        for p, n, val, rs, tag in self.V:
            if tag: out[tag] = (val-(v[p]-v[n]))/rs      # current delivered by the source
        return out

# ------------------------------------------------------------------- part models
def R_val(ref): return VALUES[ref]

def build(scn, v_prev=None, passive_only=False):
    """Assemble the circuit for a scenario. v_prev = node voltages from the previous iteration
    (behavioural elements look at them)."""
    c = Circuit()
    vp = collections.defaultdict(float, v_prev or {})
    # --- passives straight from the copper netlist
    for ref in VALUES:
        if ref.startswith("R"):
            a, b = [p["asbuilt"] for p in pads_of(ref)]
            c.add_R(a, b, R_val(ref), tag=ref)
    fa, fb = [p["asbuilt"] for p in pads_of("F1")]; c.add_R(fa, fb, 0.5, tag="F1")            # SMD050F-2 ~0.5 ohm
    ba, bb = [p["asbuilt"] for p in pads_of("FB1")]; c.add_R(ba, bb, 0.8, tag="FB1")          # BLM18AG601 0.8 ohm DCR max
    io1 = [p["asbuilt"] for p in pads_of("D1") if p["pin"] == "I/O1"]; c.add_R(io1[0], io1[1], 0.01, tag="D1.IO1")  # ESD array pass-through
    io2 = [p["asbuilt"] for p in pads_of("D1") if p["pin"] == "I/O2"]; c.add_R(io2[0], io2[1], 0.01, tag="D1.IO2")
    sp, sn = net("J3", "1"), net("J3", "2"); c.add_R(sp, sn, 8.0, tag="speaker")                    # 8 ohm speaker on J3
    for sw in ("SW1", "SW2", "SW3", "SW4"):
        if scn.get(sw) == "closed":
            a, b = [p["asbuilt"] for p in pads_of(sw)]; c.add_R(a, b, 0.05, tag=sw)
    c.add_R(net("U3", "~SD_"), GND, 100e3, tag="U3.SD_pulldown")                                     # MAX98357A SD_MODE internal pull-down (assumed)
    if scn.get("swd_run_low"): c.add_R(net("J4", "05"), GND, 1.0, tag="debugger_RUN")
    if passive_only: return c
    # --- sources
    vbus = scn.get("vbus", 5.0)
    if vbus > 0: c.add_V(net("J1", "VBUS"), GND, vbus, rs=0.1, tag="USB_source")
    # --- U5 MCP1825T-3302 LDO (behavioural)
    vin, shdn, vout_net = net("U5", "VIN"), net("U5", "~SHD"), net("U5", "VOUT")
    ldo_on = vp[shdn] > 1.5 and vp[vin] > 1.8
    if ldo_on:
        target = min(3.3, vp[vin] - 0.10)              # ~100 mV dropout at ~100 mA
        c.add_V(vout_net, GND, target, rs=0.05, tag="LDO_out")
        c.add_I(vin, GND, 0.12e-3 + max(0.0, vp["_ldo_iout"]), tag="LDO_in")   # input current = output + quiescent
    pwrgd_ok = ldo_on and vp[vout_net] >= 0.92*3.3
    if not pwrgd_ok: c.add_R(net("U5", "PWRG"), GND, 60.0, tag="LDO_PWRGD_low")   # open-drain pulled low
    # --- U1 RP2040
    iovdd = U1["IOVDD"][0]["asbuilt"]
    core_i = scn.get("core_mA", 40e-3)
    if vp[net("U1", "VREG_VIN")] > 1.6:
        c.add_V(net("U1", "VREG_VOUT"), GND, 1.10, rs=0.1, tag="RP2040_VREG")
        c.add_I(net("U1", "VREG_VIN"), GND, core_i + 1e-3, tag="RP2040_VREG_in")
    def load(n, i_nom, v_nom, tag):            # constant-current load expressed as R at nominal voltage
        c.add_R(n, GND, v_nom / i_nom, tag=tag)
    load(net("U1", "DVDD"), core_i, 1.1, "RP2040_core")
    load(iovdd, 5e-3, 3.3, "RP2040_IOVDD")
    load(net("U1", "USB_VDD"), 5e-3, 3.3, "RP2040_USB_VDD")
    load(net("U1", "ADC_AVDD"), 0.5e-3, 3.3, "RP2040_ADC_AVDD")
    gpio = dict(QSPI_SS="in_pu", SWCLK="in_pu", SWD="in_pu")       # boot-time defaults
    gpio.update(scn.get("gpio", {}))
    for name, mode in gpio.items():
        n = net("U1", name)
        if mode == "in_pu": c.add_R(n, iovdd, 56e3, tag=f"U1.{name}.pu")
        elif mode == "in_pd": c.add_R(n, GND, 56e3, tag=f"U1.{name}.pd")
        elif mode == "out_hi": c.add_V(n, GND, vp[iovdd], rs=30.0, tag=f"U1.{name}.drv")
        elif mode == "out_lo": c.add_V(n, GND, 0.0, rs=30.0, tag=f"U1.{name}.drv")
    # --- U2 flash, U3 amp, MK1 mic
    load(net("U2", "VCC"), 3e-3, 3.3, "flash")
    amp_i = {"off": 0.1e-3, "idle": 2.5e-3, "play": scn.get("amp_mA", 240e-3)}[scn.get("amp", "off")]
    load(net("U3", "VDD_"), amp_i, 5.0, "amp")
    load(net("MK1", "VDD"), 0.7e-3, 3.3, "mic")
    return c

def simulate(scn):
    v = None; ldo_iout = 0.0
    for it in range(40):
        c = build(scn, dict((v or {}), _ldo_iout=ldo_iout))
        v2 = c.solve(); br = c.branch_currents(v2)
        ldo_iout = max(0.0, br.get("LDO_out", 0.0))
        if v is not None and max(abs(v2[k]-v.get(k, 0.0)) for k in v2) < 1e-4 and abs(ldo_iout - (br.get("LDO_out", 0.0))) < 1e-6:
            v = v2; break
        v = v2
    return v, br, c

def resistance(a, b, scn=None):
    """Two-terminal resistance through the passive network (sources off, ICs unpowered)."""
    if a == b: return 0.0
    c = build(scn or {}, passive_only=True)
    c.add_I(b, a, 1.0)                       # push 1 A into a, pull from b
    v = c.solve()
    r = v[a] - v[b]
    return r

# ------------------------------------------------------------------- test harness
RESULTS = []
def check(section, name, ok, detail, warn=False):
    RESULTS.append((section, name, "PASS" if ok else ("WARN" if warn else "FAIL"), detail))
def approx(v, lo, hi): return lo <= v <= hi
def fmtR(r): return "open (>1 MΩ)" if r > 1e6 else (f"{r*1e3:.1f} mΩ" if r < 0.1 else f"{r:.3g} Ω")

def run():
    # ---------------- 0. netlist equivalence (copper vs intent)
    sec = "Netlist: as-built copper vs intended"
    check(sec, "no orphan pads / shorts / split nets", not NL["issues"], "; ".join(NL["issues"]) or "all pads on the intended nets")
    check(sec, "RP2040 pin numbering matches netlist labels", not U1_MISMATCH, "; ".join(U1_MISMATCH) or "56 pins identified from pad geometry")
    fw = {"GPIO2": "I2S_BCLK", "GPIO3": "I2S_LRCLK", "GPIO4": "I2S_DAC_DATA", "GPIO5": "PDM_DATA", "GPIO6": "PDM_CLK", "GPIO21": "AMP_SD_MODE"}
    for g, n in fw.items():
        check(sec, f"{g} = {n} (per project description)", net("U1", g) == n, f"{g} is on {net('U1', g)}")
    for g in ("GPIO7", "GPIO8", "GPIO9"):
        check(sec, f"{g} assignment", True, f"{g} is on {net('U1', g)}")
    for name in ("IOVDD", "DVDD"):
        nets = {p["asbuilt"] for p in U1[name]}
        check(sec, f"all RP2040 {name} pins on one net", len(nets) == 1, f"{name} pins: {nets}")
    ep = U1["GND"] if isinstance(U1["GND"], dict) else U1["GND"][0]
    check(sec, "RP2040 exposed pad on GND", ep["asbuilt"] == GND, f"EP is on {ep['asbuilt']}")
    check(sec, "TESTEN tied to GND", net("U1", "TESTEN") == GND, f"TESTEN on {net('U1', 'TESTEN')}")
    check(sec, "no unpowered IC supply pins", all(net(r, p) != "UNCONNECTED" for r, p in [("U2", "VCC"), ("U3", "VDD_"), ("MK1", "VDD"), ("U5", "VIN")]), "U2/U3/MK1/U5 supply pins connected")

    # ---------------- 1. two-terminal wiring paths
    sec = "Wiring paths (2-terminal resistance, ICs unpowered)"
    paths = [
        ("USB D+ : J1.DP1 -> D1 -> R3 -> RP2040 USB_DP", net("J1", "DP1"), net("U1", "USB_DP"), 27.0, 0.5),
        ("USB D- : J1.DN1 -> D1 -> R4 -> RP2040 USB_DM", net("J1", "DN1"), net("U1", "USB_DM"), 27.0, 0.5),
        ("USB D+ both connector pins tied (DP1-DP2)", net("J1", "DP1"), net("J1", "DP2"), 0.0, 0.05),
        ("USB D- both connector pins tied (DN1-DN2)", net("J1", "DN1"), net("J1", "DN2"), 0.0, 0.05),
        ("CC1 pull-down to GND (R1)", net("J1", "CC1"), GND, 5100.0, 1.0),
        ("CC2 pull-down to GND (R2)", net("J1", "CC2"), GND, 5100.0, 1.0),
        ("VBUS: J1.VBUS -> F1 -> VBUS_5V (fuse)", net("J1", "VBUS"), net("U5", "VIN"), 0.5, 0.05),
        ("VBUS_5V -> amp VDD", net("U5", "VIN"), net("U3", "VDD_"), 0.0, 0.05),
        ("VBUS_5V -> LDO SHDN (always enabled)", net("U5", "VIN"), net("U5", "~SHD"), 0.0, 0.05),
        ("3V3 -> FB1 -> mic VDD", net("U5", "VOUT"), net("MK1", "VDD"), 0.8, 0.05),
        ("3V3 -> flash VCC", net("U5", "VOUT"), net("U2", "VCC"), 0.0, 0.05),
        ("3V3 -> SWD header pin 1", net("U5", "VOUT"), net("J4", "01"), 0.0, 0.05),
        ("RP2040 VREG_VOUT -> DVDD (1V1 core)", net("U1", "VREG_VOUT"), net("U1", "DVDD"), 0.0, 0.05),
        ("XIN -> Y1", net("U1", "XIN"), min((net("Y1", "CRYS", k) for k in (0, 1)), key=lambda n: resistance(net("U1", "XIN"), n)), 0.0, 0.05),
        ("XOUT -> R5 -> Y1", net("U1", "XOUT"), min((net("Y1", "CRYS", k) for k in (0, 1)), key=lambda n: resistance(net("U1", "XOUT"), n)), 1000.0, 1.0),
        ("QSPI_SS -> R6 -> flash CS", net("U1", "QSPI_SS"), net("U2", "~{CS"), 1000.0, 1.0),
        ("flash CS pull-up R7 to 3V3", net("U2", "~{CS"), net("U5", "VOUT"), 10000.0, 1.0),
        ("QSPI_SCLK -> flash CLK", net("U1", "QSPI_SCLK"), net("U2", "CLK"), 0.0, 0.05),
        ("QSPI_SD0 -> flash DI (IO0)", net("U1", "QSPI_SD0"), net("U2", "DI(I"), 0.0, 0.05),
        ("QSPI_SD1 -> flash DO (IO1)", net("U1", "QSPI_SD1"), net("U2", "DO(I"), 0.0, 0.05),
        ("QSPI_SD2 -> flash WP# (IO2)", net("U1", "QSPI_SD2"), net("U2", "WP#("), 0.0, 0.05),
        ("QSPI_SD3 -> flash HOLD (IO3)", net("U1", "QSPI_SD3"), net("U2", "HOLD"), 0.0, 0.05),
        ("GPIO2 -> amp BCLK", net("U1", "GPIO2"), net("U3", "BCLK"), 0.0, 0.05),
        ("GPIO3 -> amp LRCLK", net("U1", "GPIO3"), net("U3", "LRCL"), 0.0, 0.05),
        ("GPIO4 -> amp DIN", net("U1", "GPIO4"), net("U3", "DIN"), 0.0, 0.05),
        ("GPIO21 -> amp SD_MODE", net("U1", "GPIO21"), net("U3", "~SD_"), 0.0, 0.05),
        ("amp OUTP -> J3 pin 1", net("U3", "OUTP"), net("J3", "1"), 0.0, 0.05),
        ("amp OUTN -> J3 pin 2", net("U3", "OUTN"), net("J3", "2"), 0.0, 0.05),
        ("speaker terminals not grounded (J3.1 -> GND)", net("J3", "1"), GND, float("inf"), 0),
        ("speaker terminals not grounded (J3.2 -> GND)", net("J3", "2"), GND, float("inf"), 0),
        ("GPIO6 -> mic CLK", net("U1", "GPIO6"), net("MK1", "CLOC"), 0.0, 0.05),
        ("GPIO5 -> mic DATA", net("U1", "GPIO5"), net("MK1", "DATA"), 0.0, 0.05),
        ("mic SELECT tied low", net("MK1", "SELE"), GND, 0.0, 0.05),
        ("SWD header: SWDIO", net("J4", "02"), net("U1", "SWD"), 0.0, 0.05),
        ("SWD header: SWCLK", net("J4", "04"), net("U1", "SWCLK"), 0.0, 0.05),
        ("SWD header: RUN", net("J4", "05"), net("U1", "RUN"), 0.0, 0.05),
        ("SWD header: GND", net("J4", "03"), GND, 0.0, 0.05),
        ("RUN pull-up R8 to 3V3", net("U1", "RUN"), net("U5", "VOUT"), 10000.0, 1.0),
        ("PWRGD pull-up R9 to 3V3", net("U5", "PWRG"), net("U5", "VOUT"), 10000.0, 1.0),
        ("PWRGD -> GPIO9", net("U5", "PWRG"), net("U1", "GPIO9"), 0.0, 0.05),
        ("F13 pull-up R14 to 3V3", net("SW1", "1"), net("U5", "VOUT"), 10000.0, 1.0),
        ("F14 pull-up R15 to 3V3", net("SW2", "1"), net("U5", "VOUT"), 10000.0, 1.0),
        ("no DC path 3V3 -> GND with everything off", net("U5", "VOUT"), GND, float("inf"), 0),
        ("no DC path VBUS_5V -> GND with everything off", net("U5", "VIN"), GND, float("inf"), 0),
        ("no DC path 1V1 -> GND with everything off", net("U1", "DVDD"), GND, float("inf"), 0),
        ("USB D+ isolated from D-", net("J1", "DP1"), net("J1", "DN1"), float("inf"), 0),
        ("USB D+ isolated from GND", net("J1", "DP1"), GND, float("inf"), 0),
    ]
    for name, a, b, expect, tol in paths:
        r = resistance(a, b)
        ok = (r > 1e6) if expect == float("inf") else abs(r - expect) <= tol
        check(sec, name, ok, f"measured {fmtR(r)}, expected {('open' if expect == float('inf') else fmtR(expect))}")
    # switches
    for sw, node, label in (("SW1", net("SW1", "1"), "SW1 shorts F13_BTN_N to GND"), ("SW2", net("SW2", "1"), "SW2 shorts F14_BTN_N to GND"),
                            ("SW3", net("SW3", "1"), f"SW3 shorts {net('SW3', '1')} to GND"), ("SW4", net("SW4", "1"), f"SW4 shorts {net('SW4', '1')} to GND")):
        r = resistance(node, GND, {sw: "closed"})
        check(sec, label, r < 0.1, f"closed: {fmtR(r)}; open: {fmtR(resistance(node, GND))}")

    # ---------------- 2. decoupling placement (geometry)
    sec = "Decoupling capacitors (net + distance to the pin they serve)"
    caps = [r for r in VALUES if r.startswith("C")]
    def cap_pads(ref): return pads_of(ref)
    def nearest_cap(pin_pad, want_net):
        best = None
        for cref in caps:
            cp = cap_pads(cref); nets_ = {p["asbuilt"] for p in cp}
            if want_net in nets_ and GND in nets_:
                for p in cp:
                    if p["asbuilt"] == want_net:
                        d = math.hypot(p["x"]-pin_pad["x"], p["y"]-pin_pad["y"])
                        if best is None or d < best[0]: best = (d, cref)
        return best
    for name in ("IOVDD", "USB_VDD", "ADC_AVDD", "VREG_VIN", "DVDD", "VREG_VOUT"):
        plist = U1[name] if isinstance(U1[name], list) else [U1[name]]
        for p in plist:
            b = nearest_cap(p, p["asbuilt"])
            check(sec, f"RP2040 pin {p['rp_pin']} {name} ({p['asbuilt']})", b is not None and b[0] <= 3.0,
                  f"nearest cap {b[1]} {VALUES[b[1]]*1e9:.0f} nF at {b[0]:.2f} mm" if b else "no capacitor on this rail", warn=True)
    for ref, pin, label in (("U2", "VCC", "flash VCC"), ("U3", "VDD_", "amp VDD"), ("MK1", "VDD", "mic VDD (filtered)"), ("U5", "VIN", "LDO input"), ("U5", "VOUT", "LDO output")):
        p = [q for q in pads_of(ref) if q["pin"] == pin][0]; b = nearest_cap(p, p["asbuilt"])
        check(sec, f"{label} ({p['asbuilt']})", b is not None and b[0] <= 4.0, f"nearest cap {b[1]} {VALUES[b[1]]*1e9:.0f} nF at {b[0]:.2f} mm" if b else "no capacitor on this rail", warn=True)
    # crystal + reset RC
    xi, xo = net("U1", "XIN"), net("R5", "P2")
    c12 = {p["asbuilt"] for p in pads_of("C12")}; c18 = {p["asbuilt"] for p in pads_of("C18")}
    check(sec, "crystal load caps 27 pF on XIN and XOUT sides", (xi in c12 and GND in c12 and xo in c18 and GND in c18) or (xo in c12 and GND in c12 and xi in c18 and GND in c18),
          f"C12 on {c12}, C18 on {c18}")
    c20 = {p["asbuilt"] for p in pads_of("C20")}
    tau = VALUES["R8"]*VALUES["C20"]
    check(sec, "RUN reset RC (R8 + C20)", net("U1", "RUN") in c20 and GND in c20, f"C20 on {c20}; tau = {tau*1e3:.1f} ms")

    # ---------------- 3. DC operating point scenarios
    sec = "DC scenarios"
    key_nets = ["USB_VBUS_RAW", "VBUS_5V", "3V3", "1V1_CORE", "MIC_3V3_FILT", "3V3_PWRGD", "MCU_RUN", "QSPI_CS_FLASH", "QSPI_SS_MCU",
                "F13_BTN_N", "F14_BTN_N", "AMP_SD_MODE", "USB_CC1", "USB_CC2", "SPKR_P", "SPKR_N"]
    scenarios = [
        ("USB plugged, idle", dict(), {
            "USB_VBUS_RAW": (4.95, 5.0), "VBUS_5V": (4.9, 5.0), "3V3": (3.25, 3.35), "1V1_CORE": (1.05, 1.15), "MIC_3V3_FILT": (3.25, 3.35),
            "3V3_PWRGD": (3.2, 3.35), "MCU_RUN": (3.2, 3.35), "QSPI_CS_FLASH": (3.2, 3.35), "QSPI_SS_MCU": (3.2, 3.35),
            "F13_BTN_N": (3.2, 3.35), "F14_BTN_N": (3.2, 3.35), "AMP_SD_MODE": (0, 0.05), "USB_CC1": (0, 0.01), "USB_CC2": (0, 0.01)}),
        ("F13 button pressed", dict(SW1="closed"), {"F13_BTN_N": (0, 0.05), "F14_BTN_N": (3.2, 3.35), "3V3": (3.25, 3.35)}),
        ("F14 button pressed", dict(SW2="closed"), {"F14_BTN_N": (0, 0.05), "F13_BTN_N": (3.2, 3.35), "3V3": (3.25, 3.35)}),
        ("SW3 pressed (RUN low = reset)", dict(SW3="closed"), {"MCU_RUN": (0, 0.05), "3V3": (3.25, 3.35), "QSPI_CS_FLASH": (3.2, 3.35)}),
        ("SW4 pressed (flash CS low = BOOTSEL)", dict(SW4="closed"), {"QSPI_CS_FLASH": (0, 0.05), "QSPI_SS_MCU": (0, 0.4), "MCU_RUN": (3.2, 3.35)}),
        ("Debugger holds RUN low via J4", dict(swd_run_low=True), {"MCU_RUN": (0, 0.05), "3V3": (3.25, 3.35)}),
        ("Amp enabled, 1 W playing (GPIO21 high)", dict(gpio={"GPIO21": "out_hi"}, amp="play"), {"AMP_SD_MODE": (3.0, 3.35), "VBUS_5V": (4.7, 5.0), "3V3": (3.25, 3.35)}),
        ("Amp disabled (GPIO21 low)", dict(gpio={"GPIO21": "out_lo"}, amp="off"), {"AMP_SD_MODE": (0, 0.05)}),
        ("USB brown-out to 3.0 V", dict(vbus=3.0), {"3V3": (2.7, 3.0), "3V3_PWRGD": (0, 0.2)}),
        ("USB unplugged", dict(vbus=0.0), {n: (0, 0.01) for n in ("VBUS_5V", "3V3", "1V1_CORE", "MCU_RUN", "3V3_PWRGD")}),
    ]
    tables = []
    for title, scn, expect in scenarios:
        v, br, _ = simulate(scn)
        rows = []
        for n in key_nets:
            val = v.get(n, 0.0); exp = expect.get(n)
            ok = exp is None or approx(val, *exp)
            rows.append((n, val, exp, ok))
            if exp is not None: check(sec, f"{title}: V({n})", ok, f"{val:.3f} V (expected {exp[0]}..{exp[1]} V)")
        i_fuse = abs(br.get("F1", 0.0)); i_ldo = br.get("LDO_out", 0.0); vin = v.get(net("U5", "VIN"), 0.0); vout = v.get(net("U5", "VOUT"), 0.0)
        p_ldo = (vin - vout) * i_ldo if i_ldo > 0 else 0.0
        if scn.get("vbus", 5.0) > 0:
            check(sec, f"{title}: fuse current below 0.5 A hold", i_fuse < 0.5, f"{i_fuse*1e3:.0f} mA through F1")
            check(sec, f"{title}: LDO within 500 mA and dissipation sane", i_ldo < 0.5 and p_ldo < 0.5, f"LDO out {i_ldo*1e3:.1f} mA, dissipation {p_ldo*1e3:.0f} mW")
        tables.append((title, rows, dict(fuse_mA=i_fuse*1e3, ldo_mA=i_ldo*1e3, ldo_mW=p_ldo*1e3, vreg_mA=br.get("RP2040_VREG", 0.0)*1e3,
                                          r14_uA=br.get("R14", 0.0)*1e6, r7_uA=br.get("R7", 0.0)*1e6, r9_uA=br.get("R9", 0.0)*1e6)))
    return tables

def report(tables):
    lines = ["# Wiring simulation results", "", f"Board: Claude Handset #3cf75256, netlist from {os.path.basename(os.environ.get('NETLIST_IN', 'asbuilt_netlist.json'))}", ""]
    n_pass = sum(1 for r in RESULTS if r[2] == "PASS"); n_fail = sum(1 for r in RESULTS if r[2] == "FAIL"); n_warn = sum(1 for r in RESULTS if r[2] == "WARN")
    lines += [f"**{n_pass} checks passed, {n_fail} failed, {n_warn} warnings (placement, not wiring).**", ""]
    cur = None
    for section, name, status, detail in RESULTS:
        if section != cur:
            cur = section; lines += ["", f"## {section}", "", "| Check | Result | Detail |", "|---|---|---|"]
        lines.append(f"| {name} | {status} | {detail} |")
    lines += ["", "## Scenario operating points", ""]
    for title, rows, extra in tables:
        lines += [f"### {title}", "", "| Net | V | expected | ok |", "|---|---|---|---|"]
        for n, val, exp, ok in rows:
            lines.append(f"| {n} | {val:.3f} | {'' if exp is None else f'{exp[0]}..{exp[1]}'} | {'ok' if ok else 'FAIL'} |")
        lines.append(f"\nfuse {extra['fuse_mA']:.1f} mA, LDO out {extra['ldo_mA']:.1f} mA ({extra['ldo_mW']:.0f} mW), RP2040 core reg {extra['vreg_mA']:.1f} mA, "
                     f"I(R14) {extra['r14_uA']:.0f} µA, I(R7) {extra['r7_uA']:.0f} µA, I(R9) {extra['r9_uA']:.0f} µA\n")
    lines += ["", "## Model assumptions", "",
              "- Sources/loads: USB 5 V with 0.1 Ω; fuse 0.5 Ω; ferrite 0.8 Ω; ESD array pass-through 10 mΩ; speaker 8 Ω.",
              "- MCP1825: 3.3 V out, 100 mV dropout, PWRGD open-drain pulls low below 92 % of 3.3 V, enabled when SHDN > 1.5 V.",
              "- RP2040: core regulator 1.1 V when VREG_VIN > 1.6 V; core 40 mA, IOVDD 5 mA, USB_VDD 5 mA, ADC 0.5 mA; QSPI_SS/SWD internal 56 kΩ pull-ups; GPIO drivers 30 Ω.",
              "- Flash 3 mA, mic 0.7 mA, amp 0.1 mA off / 240 mA at 1 W into 8 Ω; MAX98357A SD_MODE assumed to have a 100 kΩ internal pull-down.",
              "- Capacitors and the crystal are open at DC; they are checked by net membership, value and distance instead.",
              "- Two-terminal resistances are measured with all ICs unpowered and switches open unless stated."]
    txt = "\n".join(lines)
    open(os.environ.get("RESULTS_OUT", os.path.join(HERE, "results.md")), "w").write(txt)
    return txt

if __name__ == "__main__":
    tables = run()
    txt = report(tables)
    for section, name, status, detail in RESULTS:
        print(f"[{status}] {name} -- {detail}")
    print(f"\n{sum(1 for r in RESULTS if r[2]=='PASS')} passed, {sum(1 for r in RESULTS if r[2]=='FAIL')} failed, {sum(1 for r in RESULTS if r[2]=='WARN')} warnings. Full report: sim/results.md")
