#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
g++ -std=c++17 -Wall -Wextra -I src src/main.cpp src/HRMS.cpp -o peopleflow
./peopleflow
