#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
: "${ARM_CC:=arm-none-eabi-gcc}"
autoas="$root/third_party/autoas"
revision="7bf0210644a932ae45e6cabbe8388729808af7fd"
[[ -d "$autoas/.git" ]] && [[ "$(git -C "$autoas" rev-parse HEAD)" == "$revision" ]] || {
  echo "Pinned AutoAS revision is unavailable" >&2; exit 1;
}
dir="hardware/simulator/h743-autoas-tp"
port="hardware/simulator/h743-autoas"
out="build/emu/h743-autoas-tp"
mkdir -p "$out"
includes=( "-I$dir" "-I$port" "-I$autoas/infras/include"
           "-I$autoas/infras/communication/CanIf"
           "-I$autoas/infras/communication/CanTp"
           "-I$autoas/infras/communication/PduR"
           "-I$autoas/infras/libraries/mempool" )
flags=( -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c11 -Os -g3
        -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic
        -ffunction-sections -fdata-sections -DSTD_NO_ERRNO_H -DUSE_DET )
for role in tester server; do
  if [[ "$role" == tester ]]; then role_id=0; else role_id=1; fi
  "$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" -DSIM_ROLE="$role_id" \
    -c "$dir/tp_app.c" -o "$out/$role-app.o"
  "$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" -DSIM_ROLE="$role_id" \
    -c "$port/board_mcan.c" -o "$out/$role-board.o"
  "$ARM_CC" "${flags[@]}" "${includes[@]}" \
    -c "$autoas/infras/communication/CanIf/CanIf.c" -o "$out/$role-CanIf.o"
  "$ARM_CC" "${flags[@]}" "${includes[@]}" \
    -c "$autoas/infras/communication/CanTp/CanTp.c" -o "$out/$role-CanTp.o"
  for module in PduR PduR_CanTp PduR_Dcm; do
    "$ARM_CC" "${flags[@]}" "${includes[@]}" \
      -c "$autoas/infras/communication/PduR/$module.c" -o "$out/$role-$module.o"
  done
  "$ARM_CC" -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
    "-Wl,-T,hardware/simulator/h743-fdcan/memory.ld" -Wl,--gc-sections \
    hardware/simulator/h743-fdcan/startup.S \
    "$out/$role-app.o" "$out/$role-board.o" \
    "$out/$role-CanIf.o" "$out/$role-CanTp.o" \
    "$out/$role-PduR.o" "$out/$role-PduR_CanTp.o" "$out/$role-PduR_Dcm.o" \
    -lgcc -o "$out/$role.elf"
  for symbol in CanIf_RxIndication CanIf_Transmit CanTp_Transmit CanTp_RxIndication CanTp_MainFunction PduR_TpTransmit PduR_CanTpRxIndication PduR_DcmTransmit; do
    arm-none-eabi-nm "$out/$role.elf" |grep -q " T $symbol$"
  done
  arm-none-eabi-size "$out/$role.elf"
  echo "PASS: real upstream CanIf + CanTp + PduR linked: $out/$role.elf"
done
