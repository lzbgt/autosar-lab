#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/install-renode.sh >/dev/null
bash tools/build-h743-autoas-canif.sh
log="$root/build/emu/h743-autoas/renode.log"
timeout 90 "$root/third_party/renode-1.17.0/renode" \
  --disable-gui -P -1 --console --plain \
  -e "include @hardware/simulator/h743-autoas/two-node.resc; quit" >"$log" 2>&1
python3 tools/check-autoas-renode.py canif "$log"
