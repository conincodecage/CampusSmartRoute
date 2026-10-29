@echo off
setlocal
cd /d "%~dp0"
if not exist smart-campus.exe call scripts\build_windows.bat
if not exist smart-campus.exe exit /b 1
smart-campus.exe
