@echo off
setlocal
cd /d "%~dp0"
REM PeopleFlow HRMS Pro - SQLite and OpenSSL dependencies required once via MSYS2 UCRT64.
if exist "C:\msys64\ucrt64\bin\g++.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist "C:\msys64\ucrt64\include\sqlite3.h" (
  echo [SETUP NEEDED] SQLite3 for C++ was not found.
  echo Open MSYS2 UCRT64 and run:
  echo pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
  pause
  exit /b 1
)
if not exist "C:\msys64\ucrt64\include\openssl\evp.h" (
  echo [SETUP NEEDED] OpenSSL for password hashing was not found.
  echo Open MSYS2 UCRT64 and run:
  echo pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
  pause
  exit /b 1
)
where g++ >nul 2>nul
if errorlevel 1 (
  echo Please install GCC through MSYS2 UCRT64.
  pause
  exit /b 1
)
echo Building PeopleFlow HRMS Pro (C++17 + SQLite + OpenSSL)...
g++ -std=c++17 -O2 -Wall -Wextra -I src src/web_server.cpp src/Database.cpp src/HRMS.cpp -o peopleflow_web.exe -lsqlite3 -lcrypto -lws2_32
if errorlevel 1 (
  echo Compilation failed; see error above.
  pause
  exit /b 1
)
echo Launching local server at http://127.0.0.1:8765
echo NOTE: Close any older PeopleFlow server before running the upgraded one.
start "PeopleFlow RBAC + SQLite Server - KEEP OPEN" /D "%~dp0" cmd /k peopleflow_web.exe
timeout /t 2 /nobreak >nul
start "" "http://127.0.0.1:8765"
exit /b 0
