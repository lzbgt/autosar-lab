#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/install-renode.sh >/dev/null
bash tools/build-h743-fdcan.sh
renode="$root/third_party/renode-1.17.0/renode"
if [[ ! -x "$renode" ]]; then
  echo "Renode missing: install with tools/install-renode.sh" >&2
  exit 1
fi
log="$root/build/emu/h743-fdcan/renode.log"
timeout 90 "$renode" --disable-gui -P -1 --console --plain \
   -e "include @hardware/simulator/h743-fdcan/two-node.resc; quit" >"$log" 2>&1
python3 tools/check-renode-results.py h743-fdcan "$log"
echo "Renode memory trace: $log"
