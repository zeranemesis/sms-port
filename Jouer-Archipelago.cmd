@echo off
setlocal
cd /d "%~dp0"
python "%~dp0tools\archipelago\launch.py" %*
if errorlevel 1 pause
