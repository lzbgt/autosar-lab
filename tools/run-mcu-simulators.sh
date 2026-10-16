#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
echo "=== STM32F407 real Trampoline operating system (OS) ==="
bash tools/run-f407-trampoline.sh
echo "=== Two virtual STM32H743 CAN FD controllers ==="
bash tools/run-h743-fdcan.sh
echo "PASS: two real MCU firmware emulation labs, Cortex-M4 OS and Cortex-M7 CAN FD"
