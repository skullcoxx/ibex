import json, sys
W = {"$_NOT_":0.67,"$_AND_":1.33,"$_OR_":1.33,"$_NAND_":1.0,"$_NOR_":1.0,"$_ANDNOT_":1.33,"$_ORNOT_":1.33,"$_XOR_":2.33,"$_XNOR_":2.33,"$_MUX_":2.33}
FF, LATCH = 5.0, 3.0
txt = open(sys.argv[1]).read()
d = json.loads(txt[txt.index("{"):])
mod = d.get("design") or next(iter(d["modules"].values()))
cells = mod.get("num_cells_by_type", {})
seq = lambda t: ("DFF" in t) or ("LATCH" in t)
wt = lambda t: W.get(t, FF if "DFF" in t else LATCH if "LATCH" in t else 0.0)
tot = sum(wt(t) * n for t, n in cells.items())
nseq = sum(n for t, n in cells.items() if seq(t))
other = {t: n for t, n in cells.items() if t not in W and not seq(t)}
print(sys.argv[1] + ": %.0f NAND2-eq (sequential cells: %d)" % (tot, nseq))
print("unweighted cell types:", other)
