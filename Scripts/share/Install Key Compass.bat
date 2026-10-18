@echo off
setlocal
cd /d "%~dp0"
title Key Compass - Install

if not exist "%~dp0Files\install.ps1" (
    echo.
    echo Please extract the zip first: right-click it ^> Extract All... ^> Extract,
    echo then double-click "Install Key Compass.bat" in the extracted folder.
    echo.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Files\install.ps1"
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
    echo Something went wrong - see the red message above.
) else (
    echo Finished. You can close this window.
)
echo.
pause
exit /b %RESULT%
