@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>&1
if not errorlevel 1 (set "PYCMD=py") else (set "PYCMD=python")
if not exist ".venv\Scripts\python.exe" (
  echo Creating Python virtual environment...
  %PYCMD% -m venv .venv
  if errorlevel 1 (
    echo Python 3.11+ is required. Install it from https://www.python.org/downloads/
    pause
    exit /b 1
  )
)
call ".venv\Scripts\activate.bat"
python -m pip install -r requirements.txt
if errorlevel 1 (
  echo Failed to install requirements. Check your internet connection and Python installation.
  pause
  exit /b 1
)
set "APP_ENV=development"
echo.
echo PeopleFlow HRMS real web app is starting at http://127.0.0.1:8765
start "" "http://127.0.0.1:8765"
python -m uvicorn server.main:app --host 127.0.0.1 --port 8765
pause
