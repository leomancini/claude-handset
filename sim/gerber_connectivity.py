#!/usr/bin/env python3
"""Connectivity check of Flux Gerber export against IPC-D-356 netlist.
Pure python, no deps. Reports shorts, opens, and floating copper."""
import re, math, sys, collections, os

ROOT = os.environ.get("GERBER_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.environ.get("CHECK_OUT", os.path.join(os.path.dirname(os.path.abspath(__file__)), ".connectivity.pkl"))
LAYERS = [("F", "claude-handset-f_cu.gbr"), ("In1", "claude-handset-in1_cu.gbr"),
          ("In2", "claude-handset-in2_cu.gbr"), ("B", "claude-handset-b_cu.gbr")]
BOARD_CX, BOARD_CY = 40.0, -40.0

# ---------------- geometry helpers ----------------
def seg_seg_dist(a, b, c, d):
    def dot(u, v): return u[0]*v[0]+u[1]*v[1]
    def sub(u, v): return (u[0]-v[0], u[1]-v[1])
    def pt_seg(p, a, b):
        ab = sub(b, a); ap = sub(p, a); l2 = dot(ab, ab)
        t = 0 if l2 == 0 else max(0, min(1, dot(ap, ab)/l2))
        q = (a[0]+ab[0]*t, a[1]+ab[1]*t)
        return math.hypot(p[0]-q[0], p[1]-q[1])
    # intersection test
    def orient(p, q, r): return (q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0])
    o1, o2, o3, o4 = orient(a, b, c), orient(a, b, d), orient(c, d, a), orient(c, d, b)
    if ((o1 > 0) != (o2 > 0)) and ((o3 > 0) != (o4 > 0)) and o1 != 0 and o2 != 0 and o3 != 0 and o4 != 0:
        return 0.0
    return min(pt_seg(a, c, d), pt_seg(b, c, d), pt_seg(c, a, b), pt_seg(d, a, b))

def pt_in_poly(p, poly):
    x, y = p; inside = False; n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]; x2, y2 = poly[(i+1) % n]
        if (y1 > y) != (y2 > y):
            xi = x1 + (y-y1)*(x2-x1)/(y2-y1)
            if xi > x: inside = not inside
    return inside

def pt_poly_dist(p, poly):
    n = len(poly)
    return min(seg_seg_dist(p, p, poly[i], poly[(i+1) % n]) for i in range(n))

class Item:
    __slots__ = ("kind", "poly", "a", "b", "w", "layer", "bbox", "idx", "desc")
    # kind: 'cap' (capsule a-b width w) or 'poly' (polygon)
    def __init__(self, kind, layer, desc, poly=None, a=None, b=None, w=0.0):
        self.kind, self.layer, self.desc = kind, layer, desc
        self.poly, self.a, self.b, self.w = poly, a, b, w
        if kind == 'cap':
            r = w/2
            self.bbox = (min(a[0], b[0])-r, min(a[1], b[1])-r, max(a[0], b[0])+r, max(a[1], b[1])+r)
        else:
            xs = [p[0] for p in poly]; ys = [p[1] for p in poly]
            self.bbox = (min(xs), min(ys), max(xs), max(ys))

    def contains_pt(self, p, tol=0.0):
        if self.kind == 'cap':
            return seg_seg_dist(p, p, self.a, self.b) <= self.w/2 + tol
        return pt_in_poly(p, self.poly) or pt_poly_dist(p, self.poly) <= tol

def items_touch(i1, i2, tol=0.0):
    b1, b2 = i1.bbox, i2.bbox
    if b1[2] < b2[0]-tol or b2[2] < b1[0]-tol or b1[3] < b2[1]-tol or b2[3] < b1[1]-tol:
        return False
    if i1.kind == 'cap' and i2.kind == 'cap':
        return seg_seg_dist(i1.a, i1.b, i2.a, i2.b) <= (i1.w+i2.w)/2 + tol
    if i1.kind == 'poly' and i2.kind == 'cap':
        i1, i2 = i2, i1
    if i1.kind == 'cap':  # cap vs poly
        poly = i2.poly; n = len(poly); r = i1.w/2
        if pt_in_poly(i1.a, poly) or pt_in_poly(i1.b, poly): return True
        for k in range(n):
            if seg_seg_dist(i1.a, i1.b, poly[k], poly[(k+1) % n]) <= r + tol: return True
        for q in poly:
            if seg_seg_dist(q, q, i1.a, i1.b) <= r + tol: return True
        return False
    # poly vs poly
    p1, p2 = i1.poly, i2.poly
    for q in p1:
        if pt_in_poly(q, p2): return True
    for q in p2:
        if pt_in_poly(q, p1): return True
    n1, n2 = len(p1), len(p2)
    for i in range(n1):
        for j in range(n2):
            if seg_seg_dist(p1[i], p1[(i+1) % n1], p2[j], p2[(j+1) % n2]) <= tol: return True
    return False

def circle_poly(cx, cy, r, n=16):
    return [(cx+r*math.cos(2*math.pi*k/n), cy+r*math.sin(2*math.pi*k/n)) for k in range(n)]

def rect_poly(cx, cy, w, h, rot=0.0):
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    pts = [(-w/2, -h/2), (w/2, -h/2), (w/2, h/2), (-w/2, h/2)]
    return [(cx + x*c - y*s, cy + x*s + y*c) for x, y in pts]

# ---------------- gerber parser ----------------
def parse_gerber(path, layer):
    apertures = {}
    aper_func = {}
    items = []
    cur_ap = None; cur_func = None
    x = y = 0.0
    in_region = False; region_pts = []
    pending_func = None
    scale = 1e-6
    polarity = "D"
    with open(path) as f:
        data = f.read()
    # strip macro definitions bodies (we know their semantics)
    for line in data.split("\n"):
        line = line.strip()
        if not line: continue
        if line.startswith("%TA.AperFunction"):
            pending_func = line.split(",", 1)[1].rstrip("*%"); continue
        m = re.match(r"%ADD(\d+)([A-Za-z0-9]+),?(.*)\*%", line)
        if m:
            code, name, params = int(m.group(1)), m.group(2), m.group(3)
            apertures[code] = (name, params); aper_func[code] = pending_func; pending_func = None
            continue
        if line.startswith("%LPC"): polarity = "C"; continue
        if line.startswith("%LPD"): polarity = "D"; continue
        if line.startswith("%") or line.startswith("G04") or line.startswith("0 ") or re.match(r"^\d+,", line) or line.startswith("$"):
            continue
        if line.startswith("G36"):
            in_region = True; region_pts = []; continue
        if line.startswith("G37"):
            in_region = False
            if len(region_pts) >= 3:
                items.append(Item('poly', layer, "region", poly=region_pts))
            region_pts = []; continue
        m = re.match(r"^D(\d+)\*$", line)
        if m:
            cur_ap = int(m.group(1)); cur_func = aper_func.get(cur_ap); continue
        m = re.match(r"^(?:X(-?\d+))?(?:Y(-?\d+))?D0([123])\*$", line)
        if m:
            nx = x if m.group(1) is None else int(m.group(1))*scale
            ny = y if m.group(2) is None else int(m.group(2))*scale
            op = m.group(3)
            if in_region:
                if op == '2': region_pts = [(nx, ny)]
                elif op == '1': region_pts.append((nx, ny))
            else:
                name, params = apertures[cur_ap]
                if op == '1':
                    if name == 'C':
                        w = float(params)
                        if polarity == "C":
                            items.append(Item('cap', layer, "CLEAR", a=(x, y), b=(nx, ny), w=w))
                        elif w > 0 and (nx, ny) != (x, y):
                            items.append(Item('cap', layer, f"trace w{w} {cur_func}", a=(x, y), b=(nx, ny), w=w))
                        elif w > 0:
                            items.append(Item('poly', layer, f"dot w{w}", poly=circle_poly(nx, ny, w/2)))
                    else:
                        # non-circular draw: approximate with width = min dimension
                        dims = [float(v) for v in params.split("X") if re.match(r"^-?[\d.]+$", v)]
                        w = min(dims[:2]) if dims else 0.1
                        items.append(Item('cap', layer, f"trace({name}) {cur_func}", a=(x, y), b=(nx, ny), w=w))
                elif op == '3':
                    if polarity == "C":
                        if name == 'C':
                            items.append(Item('cap', layer, "CLEAR", a=(nx, ny), b=(nx, ny), w=float(params)))
                        else:
                            fi = flash_item(name, params, nx, ny, layer, cur_func); fi.desc = "CLEAR"; items.append(fi)
                    else:
                        items.append(flash_item(name, params, nx, ny, layer, cur_func))
            x, y = nx, ny
            continue
        if line in ("G01*", "G01", "M02*", "G90*", "G70*", "G71*"): continue
        # unknown line
        # print("skip", line, file=sys.stderr)
    return items

def flash_item(name, params, cx, cy, layer, func):
    desc = f"pad {name} {func}"
    if name == 'C':
        r = float(params)/2
        return Item('poly', layer, desc, poly=circle_poly(cx, cy, max(r, 0.001)))
    if name == 'R':
        w, h = [float(v) for v in params.split("X")[:2]]
        return Item('poly', layer, desc, poly=rect_poly(cx, cy, w, h))
    if name == 'O':
        w, h = [float(v) for v in params.split("X")[:2]]
        return Item('poly', layer, desc, poly=rect_poly(cx, cy, w, h))
    if name == 'RoundRect':
        v = [float(t) for t in params.split("X")]
        rr = v[0]; corners = [(v[1], v[2]), (v[3], v[4]), (v[5], v[6]), (v[7], v[8])]
        # expand corners outward by rr (approx: bbox grow)
        xs = [c[0] for c in corners]; ys = [c[1] for c in corners]
        w = max(xs)-min(xs)+2*rr; h = max(ys)-min(ys)+2*rr
        return Item('poly', layer, desc, poly=rect_poly(cx+(max(xs)+min(xs))/2, cy+(max(ys)+min(ys))/2, w, h))
    if name == 'RotRect':
        l, w, rot = [float(t) for t in params.split("X")[:3]]
        return Item('poly', layer, desc, poly=rect_poly(cx, cy, l, w, rot))
    if name.startswith('FreePoly'):
        return Item('poly', layer, desc, poly=circle_poly(cx, cy, 0.001))
    raise ValueError(name)

# ---------------- netlist parser ----------------
def parse_d356(path):
    pts = []
    for line in open(path):
        if not (line.startswith("317") or line.startswith("327")): continue
        net = line[3:17].strip(); ref = line[20:26].strip(); pin = line[27:31].strip()
        m = re.search(r"X([+-]\d+)Y([+-]\d+)", line)
        X, Y = int(m.group(1)), int(m.group(2))
        xm = BOARD_CX + X*0.00254; ym = BOARD_CY + Y*0.00254
        acc = re.search(r"A(\d\d)", line)
        through = line.startswith("317")
        pts.append(dict(net=net, ref=ref or "VIA", pin=pin, x=xm, y=ym, through=through,
                        access=(acc.group(1) if acc else "00")))
    return pts

def parse_drill(path):
    holes = []; plated = True; tool = None; tools = {}
    for line in open(path):
        line = line.strip()
        m = re.match(r"^T(\d+)C([\d.]+)", line)
        if m:
            tools[int(m.group(1))] = float(m.group(2)); continue
        if "NonPlated" in line: pass
        m = re.match(r"^T(\d+)$", line)
        if m: tool = int(m.group(1)); continue
        m = re.match(r"^X([-\d.]+)Y([-\d.]+)(G85X([-\d.]+)Y([-\d.]+))?", line)
        if m and tool is not None:
            holes.append((float(m.group(1)), float(m.group(2)), tool, tools[tool]))
            if m.group(3):
                holes.append((float(m.group(4)), float(m.group(5)), tool, tools[tool]))
    return holes, tools

# ---------------- union find ----------------
class UF:
    def __init__(self, n): self.p = list(range(n))
    def find(self, i):
        while self.p[i] != i:
            self.p[i] = self.p[self.p[i]]; i = self.p[i]
        return i
    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b: self.p[a] = b

def main():
    all_items = []
    for lname, fn in LAYERS:
        its = parse_gerber(os.path.join(ROOT, fn), lname)
        print(f"{lname}: {len(its)} items", file=sys.stderr)
        all_items += its
    for i, it in enumerate(all_items): it.idx = i
    n = len(all_items)
    uf = UF(n)
    # spatial hash per layer
    CELL = 1.0
    grid = collections.defaultdict(list)
    for it in all_items:
        x0, y0, x1, y1 = it.bbox
        for gx in range(int(math.floor(x0/CELL)), int(math.floor(x1/CELL))+1):
            for gy in range(int(math.floor(y0/CELL)), int(math.floor(y1/CELL))+1):
                grid[(it.layer, gx, gy)].append(it)
    checked = set(); touching = []
    clears = [it for it in all_items if it.desc == "CLEAR"]
    def inside_clear(b, c):
        pts_ = [b.a, b.b] if b.kind == 'cap' else b.poly
        if c.kind == 'cap':
            r = c.w/2 - (b.w/2 if b.kind == 'cap' else 0)
            return all(seg_seg_dist(p, p, c.a, c.b) <= r for p in pts_)
        m = b.w/2 if b.kind == 'cap' else 0
        return all(pt_in_poly(p, c.poly) and pt_poly_dist(p, c.poly) >= m for p in pts_)
    def blocked(a, b):
        lo, hi = (a, b) if a.idx < b.idx else (b, a)
        for c in clears:
            if c.layer == a.layer and lo.idx < c.idx < hi.idx and inside_clear(hi, c): return True
        return False
    for key, lst in grid.items():
        for i in range(len(lst)):
            for j in range(i+1, len(lst)):
                a, b = lst[i], lst[j]
                pk = (min(a.idx, b.idx), max(a.idx, b.idx))
                if pk in checked: continue
                checked.add(pk)
                if uf.find(a.idx) == uf.find(b.idx): continue
                if a.desc == "CLEAR" or b.desc == "CLEAR": continue
                if items_touch(a, b) and not blocked(a, b):
                    uf.union(a.idx, b.idx); touching.append((a.idx, b.idx))
    # through-hole connections (plated drills)
    holes, tools = parse_drill(os.path.join(ROOT, "claude-handset.drl"))
    NPTH_TOOLS = {5, 6}
    def items_at(x, y, layer=None, tol=0.0):
        gx, gy = int(math.floor(x/CELL)), int(math.floor(y/CELL))
        out = []
        for L in ([layer] if layer else [l for l, _ in LAYERS]):
            seen = set()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for it in grid.get((L, gx+dx, gy+dy), []):
                        if it.idx in seen or it.desc == "CLEAR": continue
                        seen.add(it.idx)
                        if not it.contains_pt((x, y), tol): continue
                        # copper removed later by a clear (LPC) stroke/circle does not count
                        if any(c.layer == L and c.idx > it.idx and c.contains_pt((x, y), 0.0) for c in clears): continue
                        out.append(it)
        return out
    for hx, hy, tool, dia in holes:
        if tool in NPTH_TOOLS: continue
        its = items_at(hx, hy, tol=dia/2)
        for k in range(1, len(its)):
            uf.union(its[0].idx, its[k].idx); touching.append((its[0].idx, its[k].idx, 'via', hx, hy))
    # netlist assignment
    pts = parse_d356(os.path.join(ROOT, "claude-handset.d356"))
    island_nets = collections.defaultdict(lambda: collections.defaultdict(list))
    net_islands = collections.defaultdict(set)
    unplaced = []
    for p in pts:
        layer = None if p["through"] else ("F" if p["access"] == "01" else "B")
        its = items_at(p["x"], p["y"], layer, tol=0.05)
        if not its:
            unplaced.append(p); continue
        roots = {uf.find(it.idx) for it in its}
        if len(roots) > 1:
            # a pad point touching separate islands: merge (pad copper is one piece)
            r0 = list(roots)[0]
            for r in roots: uf.union(r0, r)
        r = uf.find(its[0].idx)
        island_nets[r][p["net"]].append(f'{p["ref"]}:{p["pin"]}@({p["x"]-BOARD_CX:.2f},{p["y"]-BOARD_CY:.2f})')
        net_islands[p["net"]].add(r)
    # need to recompute roots after merges
    island_nets2 = collections.defaultdict(lambda: collections.defaultdict(list))
    for r, d in island_nets.items():
        for net, lst in d.items(): island_nets2[uf.find(r)][net] += lst
    net_islands = collections.defaultdict(set)
    for r, d in island_nets2.items():
        for net in d: net_islands[net].add(r)

    print("\n=== SHORTS (one copper island carrying several nets) ===")
    for r, d in island_nets2.items():
        nets = [k for k in d if k != "N/C"]
        if len(nets) > 1:
            print(f"island {r}: nets {nets}")
            for net in nets: print(f"   {net}: {', '.join(d[net][:12])}{' ...' if len(d[net])>12 else ''}")
    print("\n=== OPENS (net split across islands) ===")
    for net, rs in sorted(net_islands.items()):
        if net == "N/C": continue
        if len(rs) > 1:
            print(f"{net}: {len(rs)} islands")
            for r in rs: print(f"   island {r}: {', '.join(island_nets2[r][net][:12])}")
    print("\n=== netlist points with no copper under them ===")
    for p in unplaced: print(f"  {p['net']} {p['ref']}:{p['pin']} ({p['x']-BOARD_CX:.2f},{p['y']-BOARD_CY:.2f}) access={p['access']} through={p['through']}")
    # floating copper
    print("\n=== FLOATING copper (island with no net), showing islands with regions or >=3 items ===")
    members = collections.defaultdict(list)
    for it in all_items: members[uf.find(it.idx)].append(it)
    for r, lst in members.items():
        if r in island_nets2: continue
        has_region = any(it.desc == "region" for it in lst)
        if has_region or len(lst) >= 3:
            xs = [it.bbox[0] for it in lst]+[it.bbox[2] for it in lst]; ys = [it.bbox[1] for it in lst]+[it.bbox[3] for it in lst]
            layers = sorted({it.layer for it in lst})
            print(f"island {r}: {len(lst)} items layers={layers} region={has_region} bbox x[{min(xs)-BOARD_CX:.2f},{max(xs)-BOARD_CX:.2f}] y[{min(ys)-BOARD_CY:.2f},{max(ys)-BOARD_CY:.2f}]")
    # dump island membership for later queries
    print("\n=== CLEARANCE < 0.127 mm between copper of different nets (same layer) ===")
    def netof(it):
        s = set(island_nets2.get(uf.find(it.idx), {}).keys()) - {"N/C"}
        return "/".join(sorted(s)) if s else None
    def edge_dist(a, b):
        if a.kind == 'cap' and b.kind == 'cap': return seg_seg_dist(a.a, a.b, b.a, b.b) - (a.w+b.w)/2
        if a.kind == 'poly' and b.kind == 'cap': a, b = b, a
        if a.kind == 'cap':
            poly = b.poly; n = len(poly)
            if pt_in_poly(a.a, poly) or pt_in_poly(a.b, poly): return 0.0
            return min(seg_seg_dist(a.a, a.b, poly[k], poly[(k+1) % n]) for k in range(n)) - a.w/2
        p1, p2 = a.poly, b.poly
        if any(pt_in_poly(q, p2) for q in p1) or any(pt_in_poly(q, p1) for q in p2): return 0.0
        return min(seg_seg_dist(p1[i], p1[(i+1) % len(p1)], p2[j], p2[(j+1) % len(p2)]) for i in range(len(p1)) for j in range(len(p2)))
    viol = {}
    for key, lst in grid.items():
        for i in range(len(lst)):
            for j in range(i+1, len(lst)):
                a, b = lst[i], lst[j]
                if a.desc in ("CLEAR", "region") or b.desc in ("CLEAR", "region"): continue
                na, nb = netof(a), netof(b)
                if na is None or nb is None or na == nb: continue
                pk = (min(a.idx, b.idx), max(a.idx, b.idx))
                if pk in viol: continue
                bb1, bb2 = a.bbox, b.bbox
                if bb1[2] < bb2[0]-0.127 or bb2[2] < bb1[0]-0.127 or bb1[3] < bb2[1]-0.127 or bb2[3] < bb1[1]-0.127: continue
                d = edge_dist(a, b)
                if d < 0.127: viol[pk] = (d, a, b, na, nb)
    def short(it):
        if it.kind == 'cap': return f"[{it.idx}] {it.layer} {it.desc} ({it.a[0]-BOARD_CX:.2f},{it.a[1]-BOARD_CY:.2f})->({it.b[0]-BOARD_CX:.2f},{it.b[1]-BOARD_CY:.2f})"
        bb = it.bbox; return f"[{it.idx}] {it.layer} {it.desc} @({(bb[0]+bb[2])/2-BOARD_CX:.2f},{(bb[1]+bb[3])/2-BOARD_CY:.2f})"
    for d, a, b, na, nb in sorted(viol.values(), key=lambda v: v[0]):
        print(f"  {d:.3f} mm  {na} vs {nb}:  {short(a)}  |  {short(b)}")
    import pickle
    adj = collections.defaultdict(set)
    for key, lst in grid.items():
        pass
    pickle.dump(dict(items=all_items, parent=uf.p, holes=holes, pts=pts, touching=touching), open(OUT, "wb"))
    return all_items, uf, island_nets2, members, items_at

if __name__ == "__main__":
    main()
