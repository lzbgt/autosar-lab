#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/install-renode.sh >/dev/null
bash tools/build-h743-autoas-dcm.sh
python3 tools/test-autoas-dcm.py
