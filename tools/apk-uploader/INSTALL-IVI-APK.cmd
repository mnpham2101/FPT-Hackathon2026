@echo off
REM Double-click this. It runs install-ivi-apk.ps1 beside it.
REM Pass extra options through, e.g.:  INSTALL-IVI-APK.cmd -KeepTunnel
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-ivi-apk.ps1" %*
echo.
echo Press any key to close this window.
pause >nul
