#!/usr/bin/env python3
"""Step 1 of the wiring simulator: build the AS-BUILT netlist from the copper.

Every pad and via in the IPC-D-356 file is located on the copper islands extracted from the
Gerbers.  Pads that share an island are electrically one net on the real board, regardless of
what the design intended.  The result is written to asbuilt_netlist.json and compared with the
intended net names so that opens, shorts and orphan pads are reported.
"""
import os, sys, io, json, collections, contextlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("GERBER_ROOT", os.path.dirname(HERE))
import gerber_connectivity as gc

def main():
    print("parsing copper layers (this takes about two minutes) ...", flush=True)
    with contextlib.redirect_stdout(io.StringIO()):
        all_items, uf, island_nets, members, items_at = gc.main()
    pts = gc.parse_d356(os.path.join(gc.ROOT, "claude-handset.d356"))
    pads = []
    for p in pts:
        layer = None if p["through"] else ("F" if p["access"] == "01" else "B")
        its = items_at(p["x"], p["y"], layer, tol=0.05)
        island = uf.find(its[0].idx) if its else None
        pads.append(dict(ref=p["ref"], pin=p["pin"], x=round(p["x"]-gc.BOARD_CX, 3), y=round(p["y"]-gc.BOARD_CY, 3),
                         intended=p["net"], island=island, through=p["through"]))
    # name each island after the majority intended net of the pads on it
    votes = collections.defaultdict(collections.Counter)
    for p in pads:
        if p["island"] is not None and p["intended"] != "N/C": votes[p["island"]][p["intended"]] += 1
    island_name = {isl: c.most_common(1)[0][0] for isl, c in votes.items()}
    used = collections.Counter(island_name.values())
    # disambiguate split nets: second island of the same name gets a suffix
    seen = collections.Counter()
    for isl in sorted(island_name, key=lambda i: -votes[i].total()):
        n = island_name[isl]; seen[n] += 1
        if seen[n] > 1: island_name[isl] = f"{n}#{seen[n]}"
    for p in pads:
        p["asbuilt"] = island_name.get(p["island"], "N/C" if p["intended"] == "N/C" else "UNCONNECTED")
    issues = []
    for p in pads:
        if p["intended"] == "N/C": continue
        if p["asbuilt"] == "UNCONNECTED": issues.append(f"orphan pad {p['ref']}:{p['pin']} ({p['intended']}) has no copper")
        elif p["asbuilt"].split("#")[0] != p["intended"]: issues.append(f"pad {p['ref']}:{p['pin']} intended {p['intended']} but sits on {p['asbuilt']}")
    for isl, c in votes.items():
        if len(c) > 1: issues.append(f"island {island_name[isl]} carries several intended nets: {dict(c)}")
    for n, k in used.items():
        if k > 1: issues.append(f"intended net {n} is split into {k} islands")
    nets = collections.defaultdict(list)
    for p in pads: nets[p["asbuilt"]].append(f"{p['ref']}:{p['pin']}" if p["ref"] != "VIA" else f"VIA@({p['x']},{p['y']})")
    out = dict(pads=pads, nets=nets, issues=issues)
    json.dump(out, open(os.environ.get("NETLIST_OUT", os.path.join(HERE, "asbuilt_netlist.json")), "w"), indent=1)
    print(f"{len(pads)} netlist points, {len(nets)} as-built nets, {len(issues)} issues")
    for i in issues: print("  ISSUE:", i)
    return out

if __name__ == "__main__":
    main()
