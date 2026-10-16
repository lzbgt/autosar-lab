#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
ARM_CC="arm-none-eabi-gcc"
command -v "$ARM_CC" >/dev/null || { echo "Missing $ARM_CC" >&2; exit 1; }
for core in cortex-m7 cortex-m4; do
  target="build/arm-check/$core"
  mkdir -p "$target"
  for source in src/core/lab_{ecu,com,cantp,dcm}.c; do
    name="$(basename "$source" .c)"
    "$ARM_CC" -std=c11 -mcpu="$core" -mthumb -mfloat-abi=soft \
      -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic -Werror \
      -Isrc/core -c "$source" -o "$target/$name.o"
  done
  echo "PASS $core: core compiles (not linked, flashed or board verified)"
done
