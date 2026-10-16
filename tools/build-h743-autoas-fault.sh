#!/usr/bin/env bash
# Drop an ISO-TP Flow Control at the modeled M_CAN FIFO-to-CanIf boundary.
# Production/default AutoAS firmware is never modified by this test.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/build-h743-autoas-dcm.sh >/dev/null
autoas="$root/third_party/autoas"
out="$root/build/emu/h743-autoas-dcm"
app="$root/hardware/simulator/h743-autoas-dcm"
board="$root/hardware/simulator/h743-autoas"
compile_fault() {
  arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=c11 -Os -g3 \
    -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic -Werror \
    -ffunction-sections -fdata-sections -DSTD_NO_ERRNO_H -DUSE_DET \
    -DAUTOAS_TEST_DROP_FC \
    "-I$app" "-I$board" "-I$root/hardware/simulator/h743-autoas-tp" \
    "-I$autoas/infras/include" "-I$autoas/infras/communication/CanIf" \
    "-I$autoas/infras/communication/CanTp" \
    "-I$autoas/infras/communication/PduR" \
    "-I$autoas/infras/diagnostic/Dcm" \
    "-I$autoas/infras/libraries/mempool" \
    -c "$1" -o "$2"
}
compile_fault "$app/dcm_server.c" "$out/fc-loss-app.o"
compile_fault "$board/board_mcan.c" "$out/fc-loss-driver.o"
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
  "-Wl,-T,hardware/simulator/h743-fdcan/memory.ld" -Wl,--gc-sections \
  "$app/startup-systick.S" \
  "$out/fc-loss-app.o" "$out/fc-loss-driver.o" "$out/dcm_config.o" \
  "$out/CanIf.o" "$out/CanTp.o" "$out/PduR.o" "$out/PduR_CanTp.o" \
  "$out/PduR_Dcm.o" "$out/Dcm.o" "$out/Dcm_Dsd.o" "$out/Dcm_Dsl.o" \
  "$out/Dcm_Dsp.o" -lgcc -o "$out/server-fc-loss.elf"
for symbol in CanTp_MainFunction Dcm_TpTxConfirmation CanIf_RxIndication; do
  arm-none-eabi-nm "$out/server-fc-loss.elf" | grep -q " T $symbol$"
done
echo "PASS: real AutoAS CanTp/Dcm loss-injection server built without upstream modifications"
arm-none-eabi-size "$out/server-fc-loss.elf"
