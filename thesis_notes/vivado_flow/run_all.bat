@echo off
REM Runs every variant at every clock period. Each run is a separate Vivado process.
REM Edit VIVADO if vivado.bat is not on PATH, e.g.  set VIVADO=C:\Xilinx\Vivado\2026.1\bin\vivado.bat
REM Keep this folder path SHORT (for example C:\ibexfpga): Windows limits paths to 260 characters.
setlocal
if "%VIVADO%"=="" set VIVADO=vivado
set PERIODS=10.0 8.0
set VARIANTS=base aes_compact aes_lut
cd /d "%~dp0"
if not exist results mkdir results
for %%V in (%VARIANTS%) do (
  for %%P in (%PERIODS%) do (
    echo ===== %%V @ %%P ns =====
    call "%VIVADO%" -mode batch -nojournal -log "results\vivado_%%V_%%P.log" -source run_one.tcl -tclargs %%V %%P
  )
)
echo.
echo Done. See results\summary.csv
endlocal
