set -e
P=$(dirname "$(readlink -f "$0")")/pack_ibex_sources.py
PY=$HOME/miniforge3/envs/ibex/bin/python3
BA=$HOME/thesis/ibex/build/lowrisc_ibex_ibex_simple_system_0/sim-verilator
BB=$HOME/thesis/ibex-base/build/lowrisc_ibex_ibex_simple_system_0/sim-verilator
cd $HOME/thesis/yosys
$PY $P --builddir $BA --repo $HOME/thesis/ibex --out pack_compact --generic-clock-gating
$PY $P --builddir $BA --repo $HOME/thesis/ibex --out pack_lut --generic-clock-gating --lut-sbox
$PY $P --builddir $BB --repo $HOME/thesis/ibex-base --out pack_base --generic-clock-gating
for C in compact lut base; do
f=$(find pack_$C -name ibex_top.sv)
sed -i -z -E 's/logic[[:space:]]+(unused_[A-Za-z0-9_]+)[[:space:]]*=[[:space:]]*([^;]*);/logic \1;\n  assign \1 = \2;/g' $f
done
