#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra -I src src/web_server.cpp src/Database.cpp src/HRMS.cpp -o peopleflow_web -lsqlite3 -lcrypto
exec ./peopleflow_web
