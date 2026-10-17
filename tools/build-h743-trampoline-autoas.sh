#!/usr/bin/env bash
# Generate and cross-build real Trampoline OSEK OS with a 1ms periodic task.
# Uses upstream ARMv7E-M generic kernel and GOIL; ONLY the board shim is ours.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
tr="$root/third_party/trampoline"
expected="ff287023252bf73273067f00d2dbc8bbc9a20401"
if [[ ! -d "$tr/.git" || ! -d "$root/third_party/autoas/.git" ]]; then
  bash tools/fetch-upstreams.sh all
fi
[[ "$(git -C "$tr" rev-parse HEAD)" == "$expected" ]] || {
  echo "Pinned Trampoline revision mismatch" >&2; exit 1;
}
if [[ ! -f "$tr/machines/cortex-m/CMSIS_5/CMSIS/Core/Include/core_cm7.h" ]]; then
  git -C "$tr" submodule update --init --depth 1 machines/cortex-m/CMSIS_5
fi
if [[ ! -x "$tr/goil/makefile-unix/goil" ]]; then
  (cd "$tr/goil/makefile-unix" && python3 build.py all 4)
fi
# Rebuild the unchanged AutoAS modules and configuration from the same repo.
bash tools/build-h743-autoas-dcm.sh >/dev/null
out="$root/build/emu/h743-trampoline-autoas"
gen="$out/trampoline_autoas"
src="$root/hardware/simulator/h743-trampoline"
mkdir -p "$out/obj"
cp "$src/trampoline_autoas.oil" "$out/"
cp "$src/rtos_app.c" "$out/"
(
  cd "$out"
  "$tr/goil/makefile-unix/goil" \
    --target=cortex-m/armv7em/stm32f407/stm32f4discovery \
    --templates="$tr/goil/templates" trampoline_autoas.oil >goil.log
)
# Only reuse the generic ARMv7E-M code generator schema. The generated F407
# wrapper is replaced with actual CMSIS Cortex-M7 definitions at build time.
cat >"$gen/cmsis_wrapper.h" <<'EOF'
#ifndef H743_TRAMPOLINE_CMSIS_WRAPPER
#define H743_TRAMPOLINE_CMSIS_WRAPPER
#define __CM7_REV 0x0100u
#define __MPU_PRESENT 0u
#define __FPU_PRESENT 0u
#define __NVIC_PRIO_BITS 4u
typedef enum {
  NonMaskableInt_IRQn = -14, HardFault_IRQn = -13,
  MemoryManagement_IRQn = -12, BusFault_IRQn = -11,
  UsageFault_IRQn = -10, SVCall_IRQn = -5,
  DebugMonitor_IRQn = -4, PendSV_IRQn = -2,
  SysTick_IRQn = -1
} IRQn_Type;
#include "core_cm7.h"
#endif
EOF
# Do not mix the two projects' incompatible ComStack_Types and Compiler
# headers within one translation unit; the boundary consists solely of
# primitive void service APIs declared in autoas_bsw.h.
inc=( "-I$gen" "-I$tr/os" "-I$tr/machines/cortex-m"
      "-I$tr/machines/cortex-m/armv7em"
      "-I$tr/machines/cortex-m/CMSIS_5/CMSIS/Core/Include"
      "-I$tr/debug" "-I$tr/com"
      "-I$root/hardware/simulator/h743-autoas-dcm" )
autoas_inc=( "-I$root/hardware/simulator/h743-autoas-dcm"
      "-I$root/hardware/simulator/h743-autoas-tp"
      "-I$root/hardware/simulator/h743-autoas"
      "-I$root/third_party/autoas/infras/include"
      "-I$root/third_party/autoas/infras/communication/CanIf"
      "-I$root/third_party/autoas/infras/communication/CanTp"
      "-I$root/third_party/autoas/infras/communication/PduR"
      "-I$root/third_party/autoas/infras/diagnostic/Dcm"
      "-I$root/third_party/autoas/infras/libraries/mempool" )
flags=( -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -std=gnu11 -O0 -g3
        -ffreestanding -fno-builtin -fno-common
        -ffunction-sections -fdata-sections -Wno-unused-parameter )
objects=()
compile() {
  local path="$1"
  local dest="$out/obj/$(basename "$path").o"
  # Generated files and board-specific files have non-colliding basenames.
  arm-none-eabi-gcc "${flags[@]}" "${inc[@]}" -c "$path" -o "$dest"
  objects+=("$dest")
}
for name in tpl_os_kernel tpl_os_timeobj_kernel tpl_os_action tpl_os_error \
            tpl_os_os_kernel tpl_os_os tpl_os_interrupt_kernel \
            tpl_os_task_kernel tpl_os_resource_kernel tpl_os_alarm_kernel; do
  compile "$tr/os/$name.c"
done
for name in tpl_machine_cortex.c tpl_sc_handler.S tpl_startup.S; do
  compile "$tr/machines/cortex-m/$name"
done
for name in tpl_ctx_switch.S tpl_ctx_switch_under_it.S tpl_interrupts.S; do
  compile "$tr/machines/cortex-m/armv7em/$name"
done
for name in tpl_app_config.c tpl_dispatch_table.c tpl_invoque.S \
            tpl_primary_irq.S tpl_vectors.c tpl_app_interrupts.c; do
  compile "$gen/$name"
done
compile "$src/board_port.c"
compile "$src/rtos_app.c"
# Link the SAME Dcm/CanTp/CanIf source as the previously verified bare-metal
# firmware, but suppress its standalone main and SysTick handler. Genuine
# Trampoline owns these entry points in this integrated firmware.
arm-none-eabi-gcc "${flags[@]}" "${autoas_inc[@]}" -DAUTOAS_TRAMPOLINE_OS \
  -DUSE_DET -DSTD_NO_ERRNO_H -c \
  "$root/hardware/simulator/h743-autoas-dcm/dcm_server.c" \
  -o "$out/obj/autoas_bsw_runtime.o"
objects+=("$out/obj/autoas_bsw_runtime.o")
for name in dcm_config board_mcan CanIf CanTp PduR PduR_CanTp PduR_Dcm \
            Dcm Dcm_Dsd Dcm_Dsl Dcm_Dsp; do
  objects+=("$root/build/emu/h743-autoas-dcm/$name.o")
done
arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -nostdlib \
  "-Wl,-T,$src/memory.ld" -Wl,--gc-sections \
  "${objects[@]}" -lgcc -o "$out/trampoline_autoas.elf"
arm-none-eabi-nm "$out/trampoline_autoas.elf" >"$out/symbols.txt"
for name in StartOS tpl_reset_handler tpl_primary_irq_handler_SysTick \
            SysTick_Handler bsw_tick_function Dcm_MainFunction CanTp_MainFunction PduR_DcmTransmit CanIf_RxIndication; do
  grep -q " T $name$" "$out/symbols.txt"
done
echo "PASS: genuine Trampoline OSEK task + real AutoAS Dcm/CanTp/PduR/CanIf in one STM32H743 ELF"
arm-none-eabi-size "$out/trampoline_autoas.elf"
