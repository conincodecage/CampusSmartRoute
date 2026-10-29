@echo off
setlocal
cd /d "%~dp0.."
where g++ >nul 2>nul
if errorlevel 1 (
  echo ERROR: g++ was not found.
  echo Install MinGW-w64 or use Code::Blocks with a GNU C++ compiler, then add g++ to PATH.
  pause
  exit /b 1
)
g++ -std=c++17 -O2 -Wall -Wextra -static-libstdc++ -static-libgcc src\server.cpp -o smart-campus.exe -lws2_32
if errorlevel 1 (
  echo Build failed. Read the compiler messages above.
  pause
  exit /b 1
)
echo Build successful: smart-campus.exe
pause
