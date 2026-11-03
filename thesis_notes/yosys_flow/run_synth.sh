source $HOME/tools/oss-cad-suite/environment
for C in "$@"; do
cd $HOME/thesis/yosys/pack_$C
FILES=$(grep -v ibex_syn_top files.txt | tr '\n' ' ')
INCS=$(sed 's/^/-I /' incdirs.txt | tr '\n' ' ')
yosys -l core.log -p "read_slang --top ibex_top -DSYNTHESIS $INCS $FILES; synth -top ibex_top -flatten; abc -g AND,NAND,OR,NOR,XOR,XNOR,ANDNOT,ORNOT,MUX; opt_clean; tee -o core.stat.json stat -json; tee -o core.ltp.txt ltp -noff"
python3 $HOME/thesis/yosys/nand2.py core.stat.json | tee core.nand2.txt
done
