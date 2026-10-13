@echo off
rem Lance la version Aurora (rendu WebGPU : Direct3D 12 par défaut).
rem SMS_AURORA_BACKEND=vulkan ou d3d11 pour changer d'API graphique.
setlocal
cd /d "%~dp0"
set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "SMS_SETTINGS=%~dp0settings.txt"
set "SMS_BINDINGS=%~dp0bindings.txt"
set "SMS_DISC_IMAGE="
set "SMS_DISC_ROOT=%~dp0rom\GMSP01-extracted\files"
set "SMS_SAVE_DIR=%~dp0saves\card-a"
set "SMS_FRONTEND_DIR=%~dp0saves\frontend"
if not defined SMS_AURORA_BACKEND set "SMS_AURORA_BACKEND=d3d12"
set "SMS_EXE=%~dp0build\windows-64-pal-aurora\sms.exe"
if not exist "%SMS_EXE%" (
  echo Executable Aurora absent : voir aurora-bridge\README.md.
  pause
  exit /b 1
)
"%SMS_EXE%" --no-launcher %*
exit /b %errorlevel%
