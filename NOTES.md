# Setup notes (WSL2 + Ibex Simple System)

Baseline Ibex commit: 65368a4c41d8e6c2e56ce816e418e21fc4e7b328

## Environment
- Windows + WSL2, Ubuntu; repo at ~/thesis/ibex, venv at ~/thesis/venv
- Verilator 5.032 (apt), FuseSoC 2.4.3 (pip, python-requirements.txt)
- RISC-V toolchain: lowrisc-toolchain-rv32imcb-20240923-1 (GCC 10.2.0) in ~/tools

## Commands that worked
source ~/thesis/venv/bin/activate
export PATH=$HOME/tools/lowrisc-toolchain-rv32imcb-20240923-1/bin:$PATH
fusesoc --cores-root=. run --target=sim --setup --build lowrisc:ibex:ibex_simple_system
make -C examples/sw/simple_system/hello_test
./build/lowrisc_ibex_ibex_simple_system_0/sim-verilator/Vibex_simple_system --meminit=ram,examples/sw/simple_system/hello_test/hello_test.elf
cat ibex_simple_system.log

## Baseline result (hello_test)
Cycles: 477, Instructions retired: 261
