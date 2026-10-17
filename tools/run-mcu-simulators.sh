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
echo "=== Genuine AutoAS Diagnostic Communication Manager on Cortex-M7 ==="
bash tools/run-h743-autoas-dcm.sh
echo "=== Real CAN Transport Layer missing Flow Control and same-server recovery ==="
bash tools/run-h743-autoas-fault.sh
echo "=== Single STM32H743 Trampoline Operating System + genuine AutoAS diagnostics ==="
bash tools/run-h743-trampoline-autoas.sh
echo "=== Trampoline Operating System-scheduled real CAN Transport Layer timeout and same-server recovery ==="
bash tools/run-h743-trampoline-autoas-fault.sh
echo "PASS: real Trampoline OS and AutoAS stack with all UDS services, ISO-TP N_Bs timeout and same-server recovery"
