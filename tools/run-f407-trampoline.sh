#!/usr/bin/env bash
# Genuine Trampoline OSEK/AUTOSAR OS on STM32F407 (Renode Cortex-M4).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/install-renode.sh >/dev/null
os="$root/third_party/trampoline"
expected="ff287023252bf73273067f00d2dbc8bbc9a20401"
if [[ ! -d "$os/.git" ]]; then bash tools/fetch-upstreams.sh trampoline; fi
if [[ "$(git -C "$os" rev-parse HEAD)" != "$expected" ]]; then
  echo "Unexpected Trampoline revision" >&2; exit 1
fi
if [[ ! -f "$os/machines/cortex-m/CMSIS_5/CMSIS/Core/Include/core_cm4.h" ]]; then
  git -C "$os" submodule update --init --depth 1 machines/cortex-m/CMSIS_5
fi
goil="$os/goil/makefile-unix/goil"
if [[ ! -x "$goil" ]]; then
  (cd "$os/goil/makefile-unix" && python3 build.py all 4)
fi
app="$os/examples/cortex-m/armv7em/stm32f407/stm32f4discovery/blink"
if ! grep -q "autosar-lab: Cortex-M Thumb _exit" "$app/blink.c"; then
  git -C "$os" apply "$root/hardware/simulator/f407-trampoline/thumb-exit.patch"
fi
(
  cd "$app"
  "$goil" --target=cortex-m/armv7em/stm32f407/stm32f4discovery \
    --templates=../../../../../../goil/templates blink.oil
  ./make.py -s
)
test -s "$app/blink_exe"
log="$root/build/emu/f407-trampoline/renode.log"
mkdir -p "$(dirname "$log")"
timeout 90 "$root/third_party/renode-1.17.0/renode" \
  --disable-gui -P -1 --console --plain \
  -e "include @hardware/simulator/f407-trampoline/blink.resc; quit" >"$log" 2>&1
python3 tools/check-renode-results.py f407-trampoline "$log"
echo "Trampoline F407 ELF: $app/blink_exe"
