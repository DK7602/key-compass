@echo off
setlocal
cd /d "%~dp0"
title Key Compass - Build and Install

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\build.ps1" %*
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
    echo Something went wrong - see the red message above. The full details are in build-log.txt
) else (
    echo Finished. You can close this window.
)
echo.
pause
exit /b %RESULT%
