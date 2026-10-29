#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
command -v g++ >/dev/null 2>&1 || { echo "g++ not found. Install a C++17 compiler." >&2; exit 1; }
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/server.cpp -o smart-campus
printf 'Build successful: %s/smart-campus\n' "$PWD"
