@echo off
setlocal
cd /d "%~dp0"
title Key Compass - Make Shareable Package

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\make-shareable.ps1"
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
