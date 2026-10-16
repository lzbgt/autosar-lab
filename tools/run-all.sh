#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 -m compileall -q tools
python3 tools/check-docs.py
python3 tools/smoke.py --backend udp --build-dir build
python3 tools/smoke.py --backend udp --fd --build-dir build
if command -v arm-none-eabi-gcc >/dev/null; then
  bash tools/check-arm-core.sh
else
  echo "SKIP ARM compiler check (install arm-none-eabi-gcc for M4/M7 object-level check)"
fi
echo "PASS: host build, CTest, both ECU virtual backends, optional ARM compilation"
