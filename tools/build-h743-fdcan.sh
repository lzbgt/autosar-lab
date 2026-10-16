#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
: "${ARM_CC:=arm-none-eabi-gcc}"
command -v "$ARM_CC" >/dev/null || { echo "Missing $ARM_CC" >&2; exit 1; }
src="hardware/simulator/h743-fdcan"
out="build/emu/h743-fdcan"
mkdir -p "$out"
for role in sender receiver; do
  case "$role" in
    sender) value=0 ;;
    receiver) value=1 ;;
  esac
  "$ARM_CC" -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c11 \
    -DSIM_ROLE="$value" -Os -g3 -ffreestanding -fno-builtin \
    -Wall -Wextra -Wpedantic -Werror -ffunction-sections -fdata-sections \
    -nostdlib "-Wl,-T,$src/memory.ld" -Wl,--gc-sections \
    "$src/startup.S" "$src/main.c" -o "$out/$role.elf"
  arm-none-eabi-size "$out/$role.elf"
  echo "PASS: built Cortex-M7 $role firmware at $out/$role.elf"
done
