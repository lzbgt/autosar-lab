#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/install-renode.sh >/dev/null
bash tools/build-h743-trampoline-autoas.sh --with-fault
python3 tools/test-h743-trampoline-autoas-fault.py
