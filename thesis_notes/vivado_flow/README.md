# Ibex + AES V3: Vivado flow (Zedboard, xc7z020clg484-1)

Three variants are compared, each synthesized out of context (core only, no board pins) at
several clock periods:

| variant       | source                                                        |
|---------------|---------------------------------------------------------------|
| `base`        | unmodified Ibex (`crypto-ext` build)                          |
| `aes_compact` | `aes-v3` build, composite-field S-box (default)               |
| `aes_lut`     | `aes-v3` build, lookup-table S-box (`--lut-sbox`)             |

All use the FPGA register file (`RegFileFPGA`, distributed RAM) and the Xilinx clock gate (BUFGCE).
Other Ibex parameters keep the `ibex_top` defaults, which match the simulator builds
(RV32M fast, RV32B none, no I-cache, no PMP).

## 1. Department machine: pack the sources

```
source ~/thesis/env.sh
mkdir -p ~/thesis/pack
B=build/lowrisc_ibex_ibex_simple_system_0/sim-verilator

python3 pack_ibex_sources.py --builddir ~/thesis/ibex-base/$B --repo ~/thesis/ibex-base --out ~/thesis/pack/base --zip
python3 pack_ibex_sources.py --builddir ~/thesis/ibex/$B      --repo ~/thesis/ibex      --out ~/thesis/pack/aes_compact --zip
python3 pack_ibex_sources.py --builddir ~/thesis/ibex/$B      --repo ~/thesis/ibex      --out ~/thesis/pack/aes_lut --lut-sbox --zip
ls -l ~/thesis/pack/*.zip
```

Copy the three zip files and `run_one.tcl`, `run_all.bat` to the laptop.

## 2. Laptop (Windows): run Vivado

Use a short folder, e.g. `C:\ibexfpga` (Windows path limit: 260 characters).

```
C:\ibexfpga\run_one.tcl
C:\ibexfpga\run_all.bat
C:\ibexfpga\sources\base\files.txt            <- unzip base.zip here
C:\ibexfpga\sources\aes_compact\files.txt     <- unzip aes_compact.zip here
C:\ibexfpga\sources\aes_lut\files.txt         <- unzip aes_lut.zip here
```

Open a Command Prompt, load the Vivado environment (adjust the path to your install), and try ONE run first:

```
C:\Xilinx\Vivado\2026.1\settings64.bat
cd C:\ibexfpga
vivado -mode batch -source run_one.tcl -tclargs base 10.0
```

If it finishes with a line starting `=== base_10.0ns : LUTs=...`, run everything:

```
run_all.bat
```

Results: `results\summary.csv` (LUTs, FFs, DSPs, BRAM, worst slack, Fmax estimate) and per-run
reports in `results\<variant>_<period>ns\` (`util_hier.txt` shows the area per module, including the AES unit).
