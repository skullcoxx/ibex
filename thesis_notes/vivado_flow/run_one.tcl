# Out-of-context synthesis + implementation of one Ibex variant for one clock period.
#
#   vivado -mode batch -source run_one.tcl -tclargs <variant> <period_ns> [sources_root] [results_root]
#
# <variant> is a sub-folder of sources_root (default: ./sources) produced by pack_ibex_sources.py.
# Results (reports, summary.csv) go to results_root (default: ./results).
# Target: Zedboard / Zynq-7000, xc7z020clg484-1.

set part      xc7z020clg484-1
set top       ibex_syn_top
set here      [file dirname [file normalize [info script]]]

if {$argc < 2} { puts "usage: run_one.tcl <variant> <period_ns> \[sources_root\] \[results_root\]"; exit 1 }
set variant   [lindex $argv 0]
set period    [lindex $argv 1]
set src_root  [expr {$argc > 2 ? [lindex $argv 2] : [file join $here sources]}]
set res_root  [expr {$argc > 3 ? [lindex $argv 3] : [file join $here results]}]
set src_root  [file normalize $src_root]
set res_root  [file normalize $res_root]
set dir       [file join $src_root $variant]
set tag       ${variant}_${period}ns
set rdir      [file join $res_root $tag]
file mkdir $rdir

proc read_lines {path} {
  set fh [open $path r]; set data [read $fh]; close $fh
  set out {}
  foreach l [split $data "\n"] { set l [string trim $l]; if {$l ne ""} { lappend out $l } }
  return $out
}

# first integer in the first table row whose label matches (7-series report_utilization format)
proc util_val {text label} {
  set re "^\\|\\s*${label}\\s*\\|\\s*(\\d+)\\s*\\|"
  if {[regexp -line -- $re $text -> v]} { return $v }
  return "n/a"
}

proc fail {rdir msg} {
  puts "ERROR: $msg"
  set fh [open [file join $rdir FAILED.txt] w]; puts $fh $msg; close $fh
}

set files {}
foreach f [read_lines [file join $dir files.txt]] { lappend files [file join $dir $f] }
set incs {}
foreach d [read_lines [file join $dir incdirs.txt]] { lappend incs [file join $dir $d] }

# --- constraints: one clock, all other ports timed against it with zero external delay
set xdc [file join $rdir clock.xdc]
set fh [open $xdc w]
puts $fh "create_clock -name clk -period $period \[get_ports clk_i\]"
puts $fh "set_input_delay  -clock clk 0 \[remove_from_collection \[all_inputs\] \[get_ports clk_i\]\]"
puts $fh "set_output_delay -clock clk 0 \[all_outputs\]"
close $fh

set t0 [clock seconds]
puts "=== $tag : reading [llength $files] files ==="
if {[catch {
  read_verilog -sv $files
  read_xdc $xdc
  synth_design -top $top -part $part -mode out_of_context -flatten_hierarchy rebuilt \
               -include_dirs $incs -verilog_define SYNTHESIS
  report_utilization -file [file join $rdir util_synth.txt]
} err]} { fail $rdir "synthesis failed: $err"; exit 1 }

if {[catch {
  opt_design
  place_design
  phys_opt_design
  route_design
  write_checkpoint -force [file join $rdir routed.dcp]
  report_utilization              -file [file join $rdir util_impl.txt]
  report_utilization -hierarchical -file [file join $rdir util_hier.txt]
  report_timing_summary -max_paths 10 -file [file join $rdir timing_summary.txt]
  report_timing -max_paths 3 -nworst 1 -file [file join $rdir timing_worst_paths.txt]
} err]} { fail $rdir "implementation failed: $err"; exit 1 }

# --- summary numbers
set fh [open [file join $rdir util_impl.txt] r]; set ut [read $fh]; close $fh
set luts  [util_val $ut {(?:Slice|CLB) LUTs\*?}]
set lutl  [util_val $ut {LUT as Logic}]
set lutm  [util_val $ut {LUT as Memory}]
set ffs   [util_val $ut {(?:Slice|CLB) Registers}]
set dsps  [util_val $ut {DSPs}]
set bram  [util_val $ut {Block RAM Tile}]

set wns "n/a"; set fmax "n/a"
set paths [get_timing_paths -max_paths 1 -setup]
if {[llength $paths] > 0} {
  set wns [get_property SLACK [lindex $paths 0]]
  set fmax [format %.1f [expr {1000.0 / ($period - $wns)}]]
}
set secs [expr {[clock seconds] - $t0}]

set csv [file join $res_root summary.csv]
set new [expr {![file exists $csv]}]
set fh [open $csv a]
if {$new} { puts $fh "variant,period_ns,luts,lut_as_logic,lut_as_memory,ffs,dsps,bram_tiles,wns_ns,fmax_est_mhz,runtime_s" }
puts $fh "$variant,$period,$luts,$lutl,$lutm,$ffs,$dsps,$bram,$wns,$fmax,$secs"
close $fh
puts "=== $tag : LUTs=$luts FFs=$ffs DSPs=$dsps BRAM=$bram WNS=$wns Fmax~$fmax MHz (${secs}s) ==="
exit 0
