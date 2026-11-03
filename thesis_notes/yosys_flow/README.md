# Yosys synthesis flow (NAND2-equivalent area, gate depth)
Tool: Yosys 0.70+255 (OSS CAD Suite 20261028), read_slang frontend. Run `source ~/tools/oss-cad-suite/environment` first.
Flow: abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; weights in nand2.py (NAND/NOR 1, AND/OR 1.33, XOR/XNOR/MUX 2.33, NOT 0.67, FF 5).
Depth = `ltp -noff` (unweighted gate count).
Steps: bash pack_all.sh ; bash run_synth.sh compact lut base
Note: ibex_top.sv `unused_*` initializers are patched in the packed copies only (slang limitation).

| Config | NAND2-eq | Depth |
|---|---|---|
| Standalone compact | 984 | 35 |
| Standalone LUT | 1972 | 28 |
| Core base | 32496 | 134 |
| Core + compact | 33855 (+4.2%) | 135 |
| Core + LUT | 34748 (+6.9%) | 139 |
