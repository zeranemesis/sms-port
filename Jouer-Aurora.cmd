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
rem Statistiques d'images toutes les 120 images dans le journal (performances).
if not defined SMS_GX_STATS set "SMS_GX_STATS=120"
set "SMS_EXE=%~dp0build\windows-64-pal-aurora\sms.exe"
if not exist "%SMS_EXE%" (
  echo Executable Aurora absent : voir aurora-bridge\README.md.
  pause
  exit /b 1
)
set "SMS_LOG_DIR=%~dp0artifacts\aurora-runtime"
if not exist "%SMS_LOG_DIR%" mkdir "%SMS_LOG_DIR%"
set "SMS_LOG_ID=latest"
for /f %%I in ('powershell.exe -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss-fff"') do set "SMS_LOG_ID=%%I"
echo Journaux Aurora : %SMS_LOG_DIR%\%SMS_LOG_ID%
"%SMS_EXE%" --no-launcher %* > "%SMS_LOG_DIR%\%SMS_LOG_ID%-stdout.log" 2> "%SMS_LOG_DIR%\%SMS_LOG_ID%-stderr.log"
set "SMS_RESULT=%errorlevel%"
if not "%SMS_RESULT%"=="0" (
  echo Le jeu s'est arrete avec le code %SMS_RESULT%.
  echo Trace conservee : %SMS_LOG_DIR%\%SMS_LOG_ID%-stderr.log
  pause
)
exit /b %SMS_RESULT%
