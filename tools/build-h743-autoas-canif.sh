#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
: "${ARM_CC:=arm-none-eabi-gcc}"
command -v "$ARM_CC" >/dev/null
autoas="$root/third_party/autoas"
revision="7bf0210644a932ae45e6cabbe8388729808af7fd"
[[ -d "$autoas/.git" ]] && [[ "$(git -C "$autoas" rev-parse HEAD)" == "$revision" ]] || {
  echo "Expected pinned real AutoAS checkout: $revision" >&2; exit 1;
}
dir="hardware/simulator/h743-autoas"
out="build/emu/h743-autoas"
mkdir -p "$out"
includes=( "-I$dir" "-I$autoas/infras/include" "-I$autoas/infras/communication/CanIf"
           "-I$autoas/infras/libraries/mempool" )
flags=( -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c11 -Os -g3
        -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic
        -ffunction-sections -fdata-sections -DSTD_NO_ERRNO_H -DUSE_DET )
for role in sender receiver; do
  if [[ "$role" == sender ]]; then role_id=0; else role_id=1; fi
  for file in autoas_canif_app board_mcan; do
    "$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" -DSIM_ROLE="$role_id" \
      -c "$dir/$file.c" -o "$out/$role-$file.o"
  done
  "$ARM_CC" "${flags[@]}" "${includes[@]}" \
    -c "$autoas/infras/communication/CanIf/CanIf.c" -o "$out/$role-real-autoas-CanIf.o"
  "$ARM_CC" -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
    "-Wl,-T,hardware/simulator/h743-fdcan/memory.ld" -Wl,--gc-sections \
    hardware/simulator/h743-fdcan/startup.S \
    "$out/$role-autoas_canif_app.o" "$out/$role-board_mcan.o" \
    "$out/$role-real-autoas-CanIf.o" -lgcc -o "$out/$role.elf"
  arm-none-eabi-nm "$out/$role.elf" |grep -q " T CanIf_RxIndication"
  arm-none-eabi-nm "$out/$role.elf" |grep -q " T CanIf_Transmit"
  arm-none-eabi-size "$out/$role.elf"
  echo "PASS real upstream AutoAS CanIf in $out/$role.elf"
done
