@echo off
rem Integration experiment: original generator, progressive abilities, separate test save.
setlocal
set "SMS_RANDOMIZER_MODE=classic-test"
set "SMS_STAGE_TOUR="
set "SMS_STAGE_TOUR_FRAMES="
set "SMS_STAGE_TOUR_LOG="
call "%~dp0Jouer-Aurora.cmd"
exit /b %errorlevel%
