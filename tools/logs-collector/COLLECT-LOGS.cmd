@echo off
REM Double-click this. It runs Collect-Logs.ps1 beside it.
REM With no options it asks which test to collect. Pass options through, e.g.:
REM   COLLECT-LOGS.cmd -Test 2
REM   COLLECT-LOGS.cmd -Blueprint phase0_smoked_test -Tail 20000
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-Logs.ps1" %*
REM Hold the script's status across the pause below, so a caller still sees
REM 0/1/2 rather than the exit code of `pause`.
set "RC=%ERRORLEVEL%"
echo.
echo Press any key to close this window.
pause >nul
exit /b %RC%
