#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
echo "=== STM32F407 real Trampoline operating system (OS) ==="
bash tools/run-f407-trampoline.sh
echo "=== Two virtual STM32H743 CAN FD controllers ==="
bash tools/run-h743-fdcan.sh
echo "=== Genuine AutoAS CAN Interface, CAN Transport Layer, Protocol Data Unit Router ==="
bash tools/run-real-autoas-mcu.sh
echo "PASS: Cortex-M4 Trampoline OS; Cortex-M7 CAN FD; genuine AutoAS CanIf, CanTp and PduR"
