@echo off
REM Double-click this. It runs Extract-Pcap.ps1 beside it.
REM The log file or run folder is required -- double-clicked with nothing to work
REM on, PowerShell asks for it. Pass it as an argument instead, e.g.:
REM   EXTRACT-PCAP.cmd .\test-report\system-test
REM   EXTRACT-PCAP.cmd .\test-report\system-test\node-v2x-ecu.txt -OutDir .\pcaps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Extract-Pcap.ps1" %*
REM Hold the script's status across the pause below, so a caller still sees
REM 0/1/2/3 rather than the exit code of `pause`.
set "RC=%ERRORLEVEL%"
echo.
echo Press any key to close this window.
pause >nul
exit /b %RC%
