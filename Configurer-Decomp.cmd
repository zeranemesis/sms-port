@echo off
setlocal
cd /d "%~dp0decomp"
if exist "C:\Users\BEAVSN\AppData\Local\Programs\Python\Python312\python.exe" (
  "C:\Users\BEAVSN\AppData\Local\Programs\Python\Python312\python.exe" configure.py --version GMSP01
) else (
  python configure.py --version GMSP01
)
exit /b %errorlevel%
