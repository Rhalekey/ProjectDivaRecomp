@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\build.ps1" -Seconds 0 -SkipTests -Player
pause
