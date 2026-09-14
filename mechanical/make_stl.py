#!/usr/bin/env python3
"""Build STL models of the assembled Claude Handset board for enclosure design.

Outputs (all in millimetres, board centre at X=Y=0, board top surface at Z=0, +Z = component side):
  claude-handset-board.stl        the 60 mm round PCB only (Z from -1.6 to 0)
  claude-handset-assembly.stl     PCB + simplified component bodies (boxes/cylinders)
  claude-handset-keepouts.stl     cylinders where the enclosure must stay open: the mic port,
                                  the side-button plungers, the USB-C opening, the speaker header
Positions and rotations come from pick_and_place.csv; body sizes from the manufacturer packages
(see BODIES below). Hole positions come from the drill file.
"""
import csv, math, os, struct, re

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
T = 1.6            # board thickness
R = 30.0           # board radius
# body (length along part X, width along part Y, height) in mm and shape
BODIES = {
    "SMD_0603": (1.6, 0.8, 0.8, "box"), "SMD_0805": (2.0, 1.25, 1.3, "box"), "SMD_0402": (1.0, 0.5, 0.5, "box"),
    "C24_0603": (1.6, 0.8, 0.8, "box"),
    "U1": (7.0, 7.0, 0.9, "box"),           # RP2040 QFN-56
    "U2": (3.0, 2.0, 0.6, "box"),           # W25Q16 USON-8
    "U3": (3.0, 3.0, 0.75, "box"),          # MAX98357A TQFN-16
    "U5": (6.5, 3.5, 1.65, "box"),          # MCP1825 SOT-223 (body only; leads add ~0.5 each side)
    "D1": (2.9, 1.6, 1.1, "box"),           # USBLC6 SOT-23-6
    "D2": (2.7, 1.8, 1.0, "box"),           # SMF5.0A SOD-123FL
    "F1": (7.4, 5.1, 3.0, "box"),           # SMD050F-2 PTC fuse (2920)
    "FB1": (1.6, 0.8, 0.8, "box"),
    "J1": (8.94, 7.35, 3.26, "box"),        # GCT USB4105 USB-C receptacle
    "J3": (7.9, 6.0, 4.5, "box"),           # JST S2B-PH-K right-angle header, opens toward -Y of the part
    "J4": (5.3, 3.5, 5.9, "box"),           # Samtec FTSH-103-01-L-DV 2x3 header
    "MK1": (3.5, 2.65, 0.98, "box"),        # Knowles SPH0641LU4H-1 (bottom port)
    "SW1": (3.0, 2.5, 1.6, "box"), "SW2": (3.0, 2.5, 1.6, "box"),   # Omron B3U-3000P side-actuated
    "SW3": (3.9, 3.0, 2.0, "box"), "SW4": (3.9, 3.0, 2.0, "box"),   # TS-1088 tactile
    "Y1": (3.2, 2.5, 0.8, "box"),           # ABM8 crystal
}

def read_pnp():
    parts = []
    with open(os.path.join(ROOT, "pick_and_place.csv"), encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            x = float(r["Mid X"].replace("mm", "")); y = float(r["Mid Y"].replace("mm", ""))
            rot = float(r["Rotation"]); ref = r["Designator"]; pkg = r["Package"]
            key = ref if ref in BODIES else ("SMD_0402" if "0402" in pkg else "SMD_0805" if "0805" in pkg else "SMD_0603")
            parts.append((ref, x, y, rot, BODIES[key]))
    return parts

def read_holes():
    holes = []; tool = None; tools = {}
    for line in open(os.path.join(ROOT, "claude-handset.drl")):
        line = line.strip()
        m = re.match(r"^T(\d+)C([\d.]+)", line)
        if m: tools[int(m.group(1))] = float(m.group(2)); continue
        m = re.match(r"^T(\d+)$", line)
        if m: tool = int(m.group(1)); continue
        m = re.match(r"^X([-\d.]+)Y([-\d.]+)", line)
        if m and tool: holes.append((float(m.group(1)) - 40.0, float(m.group(2)) + 40.0, tools[tool], tool in (5, 6)))
    return holes

# ---------------------------------------------------------------- mesh helpers
def box(cx, cy, z0, lx, ly, h, rot_deg=0.0):
    c, s = math.cos(math.radians(rot_deg)), math.sin(math.radians(rot_deg))
    def p(x, y, z): return (cx + x*c - y*s, cy + x*s + y*c, z)
    hx, hy = lx/2, ly/2
    v = [p(-hx, -hy, z0), p(hx, -hy, z0), p(hx, hy, z0), p(-hx, hy, z0), p(-hx, -hy, z0+h), p(hx, -hy, z0+h), p(hx, hy, z0+h), p(-hx, hy, z0+h)]
    faces = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return [(v[a], v[b], v[c_]) for a, b, c_ in faces]

def cylinder(cx, cy, z0, r, h, n=96):
    tris = []
    ring0 = [(cx + r*math.cos(2*math.pi*i/n), cy + r*math.sin(2*math.pi*i/n), z0) for i in range(n)]
    ring1 = [(x, y, z0+h) for x, y, _ in ring0]
    for i in range(n):
        j = (i+1) % n
        tris.append((ring0[i], ring0[j], ring1[j])); tris.append((ring0[i], ring1[j], ring1[i]))
        tris.append(((cx, cy, z0), ring0[j], ring0[i])); tris.append(((cx, cy, z0+h), ring1[i], ring1[j]))
    return tris

def write_stl(path, tris):
    with open(path, "wb") as f:
        f.write((b"claude-handset " + os.path.basename(path).encode()).ljust(80)[:80]); f.write(struct.pack("<I", len(tris)))
        for a, b, c in tris:
            ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]; vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
            nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx; l = math.hypot(nx, ny, nz) or 1.0
            f.write(struct.pack("<3f", nx/l, ny/l, nz/l))
            for pnt in (a, b, c): f.write(struct.pack("<3f", *pnt))
            f.write(b"\0\0")

def main():
    parts = read_pnp(); holes = read_holes()
    board = cylinder(0, 0, -T, R, T, n=360)
    comps = []
    for ref, x, y, rot, (lx, ly, h, shape) in parts:
        comps += box(x, y, 0.0, lx, ly, h, rot)
    keep = []
    for ref, x, y, rot, (lx, ly, h, shape) in parts:
        if ref in ("SW1", "SW2"):                       # side-actuated plunger: 1.5 mm past the body, along part -Y
            c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
            px, py = x + (0)*c - (-(ly/2 + 0.75))*s, y + (0)*s + (-(ly/2 + 0.75))*c
            keep += box(px, py, 0.0, 1.2, 1.5, h, rot)
        if ref == "J1": keep += box(x, y - 4.0, 0.0, 9.5, 8.0, 3.6, 0)      # USB-C plug opening toward the board edge (-Y)
        if ref == "J3": keep += box(x + 6.0, y, 0.0, 6.0, 7.9, 4.5, 0)      # PH cable exits along +X (header rotated 90)
        if ref == "J4": keep += box(x, y, 0.0, 5.3, 3.5, 5.9, rot)         # debug header, keep clear
    for x, y, d, npth in holes:
        if npth: keep += cylinder(x, y, -T - 2.0, d/2 + 0.5, T + 2.0)     # mic port and other NPTH: keep open below the board
    os.makedirs(HERE, exist_ok=True)
    write_stl(os.path.join(HERE, "claude-handset-board.stl"), board)
    write_stl(os.path.join(HERE, "claude-handset-assembly.stl"), board + comps)
    write_stl(os.path.join(HERE, "claude-handset-keepouts.stl"), keep)
    zmax = max(h for _, _, _, _, (_, _, h, _) in parts)
    tallest = sorted(parts, key=lambda p: -p[4][2])[:5]
    print(f"board: 60.0 mm diameter, {T} mm thick; {len(parts)} components; tallest {zmax} mm")
    for ref, x, y, rot, (lx, ly, h, _) in tallest: print(f"  {ref:4s} at ({x:+.2f},{y:+.2f}) rot {rot:.0f}: {lx}x{ly}x{h} mm")
    print("NPTH holes:", [(x, y, d) for x, y, d, n in holes if n])

if __name__ == "__main__":
    main()
