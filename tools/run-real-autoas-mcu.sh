#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
echo "=== Real AutoAS CAN Interface on two emulated STM32H743s ==="
bash tools/run-h743-autoas-canif.sh
echo "=== Real AutoAS CAN Interface + CAN Transport Layer + Protocol Data Unit Router ==="
bash tools/run-h743-autoas-tp.sh
echo "PASS: authentic AutoAS CanIf, CanTp and PduR executed on emulated Cortex-M7 CAN network"
