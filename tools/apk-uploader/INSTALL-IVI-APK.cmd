@echo off
REM Double-click this. It runs install-ivi-apk.ps1 beside it.
REM Pass extra options through, e.g.:  INSTALL-IVI-APK.cmd -KeepTunnel
REM
REM The tunnel stays OPEN while this window is up, so the log collector can run against
REM it from another terminal. It is closed below, after the keypress. -KeepTunnel keeps
REM it alive past that; -CloseTunnel closes it during the run instead.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-ivi-apk.ps1" %*
REM Hold the script's status across the pause and the teardown, so a caller still sees
REM 0/1/2 rather than the exit code of whatever ran last.
set "RC=%ERRORLEVEL%"
echo.
echo Press any key to close this window.
pause >nul

REM The script writes the port it left open here; no file means it wants no teardown.
set "HANDOVER=%~dp0logs\tunnel-open-port.txt"
if exist "%HANDOVER%" (
    set /p TUNNELPORT=<"%HANDOVER%"
    call :closetunnel
    del "%HANDOVER%" >nul 2>&1
)
exit /b %RC%

:closetunnel
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p=%TUNNELPORT%; Get-NetTCPConnection -LocalPort $p -State Listen -ErrorAction SilentlyContinue | Select-Object -ExpandProperty OwningProcess -Unique | ForEach-Object { Stop-Process -Id $_ -Force -ErrorAction SilentlyContinue }"
echo ADB tunnel is closed.
goto :eof
