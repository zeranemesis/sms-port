@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build-aurora-pal.ps1" %*
exit /b %errorlevel%
