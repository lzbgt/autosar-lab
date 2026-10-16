#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/setup-autoas.sh
if [[ ! -s third_party/autoas/app/app/config/Com/GEN/Com_Cfg.c ]]; then
    bash tools/build-autoas.sh
fi
exec "$root/.venv-autoas/bin/python" "$root/tools/generate-autoas-rte.py"
