@echo off
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 (
  echo C++ compiler g++ not found. Install MinGW-w64 or use CMake.
  pause
  exit /b 1
)
g++ -std=c++17 -Wall -Wextra -I src src/main.cpp src/HRMS.cpp -o peopleflow.exe
if errorlevel 1 (
  echo Compilation failed.
  pause
  exit /b 1
)
peopleflow.exe
pause
