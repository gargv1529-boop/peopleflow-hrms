#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
export APP_ENV=development
exec python -m uvicorn server.main:app --host 127.0.0.1 --port 8765
