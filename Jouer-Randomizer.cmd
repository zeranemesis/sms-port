@echo off
rem Compatibility shortcut: choose Randomizer on/off in the native Mods menu.
setlocal
set "SMS_STAGE_TOUR="
set "SMS_STAGE_TOUR_FRAMES="
set "SMS_STAGE_TOUR_LOG="
call "%~dp0Jouer-Aurora.cmd"
exit /b %errorlevel%
