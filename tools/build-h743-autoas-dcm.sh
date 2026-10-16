#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/build-h743-autoas-tp.sh
: "${ARM_CC:=arm-none-eabi-gcc}"
autoas="$root/third_party/autoas"
dcm="$root/hardware/simulator/h743-autoas-dcm"
out="$root/build/emu/h743-autoas-dcm"
mkdir -p "$out"
includes=( "-I$dcm"
           "-I$root/hardware/simulator/h743-autoas-tp"
           "-I$root/hardware/simulator/h743-autoas"
           "-I$autoas/infras/include"
           "-I$autoas/infras/communication/CanIf"
           "-I$autoas/infras/communication/CanTp"
           "-I$autoas/infras/communication/PduR"
           "-I$autoas/infras/diagnostic/Dcm"
           "-I$autoas/infras/libraries/mempool" )
flags=( -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c11 -Os -g3
        -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic
        -ffunction-sections -fdata-sections -DSTD_NO_ERRNO_H -DUSE_DET )
for local_source in dcm_server dcm_config; do
  "$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" \
    -c "$dcm/$local_source.c" -o "$out/$local_source.o"
done
"$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" \
  -c hardware/simulator/h743-autoas/board_mcan.c -o "$out/board_mcan.o"
for entry in "communication/CanIf/CanIf" "communication/CanTp/CanTp" \
             "communication/PduR/PduR" "communication/PduR/PduR_CanTp" \
             "communication/PduR/PduR_Dcm" "diagnostic/Dcm/Dcm" \
             "diagnostic/Dcm/Dcm_Dsd" "diagnostic/Dcm/Dcm_Dsl" \
             "diagnostic/Dcm/Dcm_Dsp"; do
  base="${entry##*/}"
  "$ARM_CC" "${flags[@]}" "${includes[@]}" \
    -c "$autoas/infras/$entry.c" -o "$out/$base.o"
done
"$ARM_CC" -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
  "-Wl,-T,hardware/simulator/h743-fdcan/memory.ld" -Wl,--gc-sections \
  hardware/simulator/h743-autoas-dcm/startup-systick.S \
  "$out/dcm_server.o" "$out/dcm_config.o" "$out/board_mcan.o" \
  "$out/CanIf.o" "$out/CanTp.o" "$out/PduR.o" "$out/PduR_CanTp.o" \
  "$out/PduR_Dcm.o" "$out/Dcm.o" "$out/Dcm_Dsd.o" "$out/Dcm_Dsl.o" \
  "$out/Dcm_Dsp.o" -lgcc -o "$out/server.elf"
for symbol in CanIf_RxIndication CanTp_RxIndication PduR_DcmTransmit \
              Dcm_Init Dcm_MainFunction Dcm_DspReadDataByIdentifier \
              Dcm_DspSessionControl; do
  arm-none-eabi-nm "$out/server.elf" |grep -q " T $symbol$"
done
arm-none-eabi-size "$out/server.elf"
echo "PASS: real upstream AutoAS Dcm + Dsd + Dsl + Dsp + PduR + CanTp + CanIf"
echo "ELF: $out/server.elf"
# Reuse already-verified genuine PduR/CanTp/CanIf tester objects.
# Only the request and expected response change. The default tester is unchanged.
for test in session reject; do
  case "$test" in
    session) test_macro="AUTOAS_TEST_SESSION" ;;
    reject) test_macro="AUTOAS_TEST_UNKNOWN_DID" ;;
  esac
  "$ARM_CC" "${flags[@]}" -Werror "${includes[@]}" -DSIM_ROLE=0 \
    "-D$test_macro" \
    -c hardware/simulator/h743-autoas-tp/tp_app.c \
    -o "$out/tester-$test.o"
  prefix="$root/build/emu/h743-autoas-tp/tester"
  "$ARM_CC" -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
    "-Wl,-T,hardware/simulator/h743-fdcan/memory.ld" -Wl,--gc-sections \
    hardware/simulator/h743-fdcan/startup.S \
    "$out/tester-$test.o" "$prefix-board.o" \
    "$prefix-CanIf.o" "$prefix-CanTp.o" "$prefix-PduR.o" \
    "$prefix-PduR_CanTp.o" "$prefix-PduR_Dcm.o" \
    -lgcc -o "$out/tester-$test.elf"
  echo "PASS: independent real-CanTp Dcm tester $test"
done
