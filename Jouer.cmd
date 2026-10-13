@echo off
setlocal
cd /d "%~dp0"
set "SMS_SETTINGS=%~dp0settings.txt"
set "SMS_BINDINGS=%~dp0bindings.txt"
set "SMS_DISC_IMAGE="
set "SMS_DISC_ROOT=%~dp0rom\GMSP01-extracted\files"
set "SMS_SAVE_DIR=%~dp0saves\card-a"
set "SMS_FRONTEND_DIR=%~dp0saves\frontend"
set "SMS_EXE=%~dp0build\windows-64-pal\sms.exe"
if exist "%~dp0build\windows-64-pal-unified\sms.exe" set "SMS_EXE=%~dp0build\windows-64-pal-unified\sms.exe"
if not exist "%SMS_EXE%" (
  echo Executable absent. Utiliser Construire-PC.cmd.
  pause
  exit /b 1
)
"%SMS_EXE%" %*
exit /b %errorlevel%
