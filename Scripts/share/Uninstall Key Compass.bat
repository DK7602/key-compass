@echo off
setlocal
cd /d "%~dp0"
title Key Compass - Uninstall

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Files\install.ps1" -Uninstall
set "RESULT=%ERRORLEVEL%"

echo.
pause
exit /b %RESULT%
