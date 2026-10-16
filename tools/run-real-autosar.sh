#!/usr/bin/env bash
# A reproducible real-stack demonstration, distinct from the small teaching ECU.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/build-autoas.sh
python3 tools/test-autoas.py
bash tools/generate-autoas-rte.sh
# Trampoline is a separate real OSEK/AUTOSAR OS, not linked to AutoAS.
bash tools/run-trampoline-posix.sh
echo "PASS: real AutoAS BSW, generated SWC Runtime Environment headers/ARXML, and separate Trampoline OS"
