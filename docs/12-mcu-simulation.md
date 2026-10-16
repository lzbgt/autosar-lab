# 12 — Run real STM32 firmware, an operating system and a CAN FD bus in emulators

This chapter is a **hardware-level stage**, not another Linux-only imitation of Controller Area Network (CAN) frames. It compiles ARM Cortex-M firmware and executes its instructions against peripheral models in [Renode](https://renode.io/), including a real STMicroelectronics General-Purpose Input/Output (GPIO) peripheral model and an M_CAN controller with message RAM.

The simulator is free and installed **inside the project**, rather than requiring a system-wide tool installation.

## Step 1 — Install the simulator and embedded compiler

~~~sh
cd ~/work/autosar-lab
bash tools/install-renode.sh
arm-none-eabi-gcc --version
~~~

The installer downloads the official Renode **1.17.0 Linux portable** release and validates the published SHA-256 of its ~60 MiB archive before extraction to the ignored `third_party/renode-1.17.0/` directory. It is idempotent and requires no root. The ARM GNU C compiler `arm-none-eabi-gcc` is required to compile the firmware.

These labs use two different target models:

| Firmware target | Renode emulated target | What is actually demonstrated |
| --- | --- | --- |
| Trampoline OS `blink_exe` | STM32F407 Discovery, Cortex-M4 | Actual Trampoline OSEK/AUTOSAR-aligned Operating System (OS) task and alarm scheduling, SysTick and periodic GPIO LED output |
| `sender.elf` + `receiver.elf` | Two separate STM32H743 Cortex-M7 instances | Memory-mapped Bosch M_CAN controller model, 16-byte CAN FD transmit/receive, DLC and Bit Rate Switch (BRS) flags, plus Classical CAN reply |

**Part compatibility matters:** The existing Renode STM32H743 platform is **not** a register-accurate model of your exact STM32H723 board. The two parts share Cortex-M7 and FDCAN concepts, but clocks, memory, peripherals, board pinout and startup may differ. The AT32M416 is not simulated in either exercise.

## Step 2 — Run genuine AUTOSAR-aligned OS firmware on STM32F407

~~~sh
bash tools/run-f407-trampoline.sh
~~~

The script performs all of the following:

1. Verifies pinned [Trampoline](https://github.com/TrampolineRTOS/trampoline) source revision `ff287023252bf73273067f00d2dbc8bbc9a20401`.
2. Installs its pinned ARM CMSIS (Cortex Microcontroller Software Interface Standard) `CMSIS_5` submodule, which provides the Cortex-M4 `core_cm4.h` declarations.
3. Runs Trampoline's **GOIL** (Trampoline OSEK Implementation Language configuration generator) against `blink.oil`, which declares a periodic alarm and the `blink` task, each with a 250-tick activation period and a 1 ms intended SysTick counter.
4. Builds a real bare-metal Cortex-M4 ELF executable using GNU Arm tools and a vendor-specific startup/linker configuration.
5. Boots the ELF on a Renode STM32F407 Discovery model, samples GPIO port D's output register at `0x40020C14` nine times and checks that the blue LED bit (PD15) toggles repeatedly.

The verified result: **four PD15 transitions across nine samples over 2.7 simulated seconds**. This is independent evidence that the task/OS timing path actually executes on a virtual ARM CPU.

A small, explicit compatibility patch is applied to the checked-out Trampoline `blink.c` only if absent. The pinned legacy example links unsuccessfully with the installed GNU Arm 16.2 linker because it resolves its post-return `_exit` destination incompatibly; the patch defines a Cortex-M Thumb-safe, nonreturning fallback reached only if `StartOS` returns. See `hardware/simulator/f407-trampoline/thumb-exit.patch`. It is not a substitute operating system or a change to scheduling logic.

The source remains in the ignored Trampoline checkout. Generated startup files, configuration and the ELF do **not** go into the main Git repository.

## Step 3 — Run a two-controller CAN FD network with real ARM firmware

~~~sh
bash tools/run-h743-fdcan.sh
~~~

This builds two different ARM Cortex-M7 applications from the checked-in C and startup source at `hardware/simulator/h743-fdcan/`. Both run on separate STM32H743 machine instances in Renode.

~~~text
STM32H743 #1                            STM32H743 #2
Cortex-M7 + FDCAN1                    Cortex-M7 + FDCAN1
  sender.elf                             receiver.elf
     │                                      │
     └──── Renode internal CAN hub ────────┘
~~~

Real test sequence:

| Step | Emulated CAN identifier | Data/behavior | Verification |
| --- | --- | --- | --- |
| 1 | `0x201` | Sender writes M_CAN message RAM: **16 data bytes** `10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F` | Flexible Data-rate Format (FDF) flag `FDF=1`, Bit Rate Switch (BRS) flag `BRS=1`, Data Length Code (DLC) `DLC=10` |
| 2 | `0x201` | Receiver's actual M_CAN model transfers the frame to Receive FIFO 0; its Cortex-M7 firmware polls/dequeues it and checks ID, DLC, flags and payload | Receiver writes `0xCAFEFD10` to reserved SRAM `0x24000000` |
| 3 | `0x202` | Receiver transmits an 8-byte **Classical CAN** frame `11 22 33 44 55 66 77 88` | Frame has FDF=0 and DLC=8 |
| 4 | `0x202` | Sender's controller receives the reply; Cortex-M7 firmware checks the payload, ID and frame format | Sender writes `0xCAFECC08` to reserved SRAM `0x24000000`; transmit complete bit is set |

The result checker `tools/check-renode-results.py` reads the statuses **from Renode's emulated SRAM**. It fails the test if either CPU failed to receive, if the CAN FD Data Length Code or BRS flag is wrong, or if the Classical reply was not received. A Renode process exiting successfully alone is not accepted as a passing test.

The C firmware uses actual Bosch M_CAN register programming: controller configuration, Receive FIFO configuration/status/acknowledgment, transmit-buffer configuration, message-RAM element headers and `TXBAR` transmit request. The memory map is specified in `memory.ld`; firmware binaries are built with strict compiler warnings.

## Step 4 — Run both emulator labs in one command

~~~sh
bash tools/run-mcu-simulators.sh
~~~

The artifacts and raw emulator logs remain under the ignored `build/emu/` directory. You can inspect the results directly:

~~~sh
python3 tools/check-renode-results.py f407-trampoline \
  build/emu/f407-trampoline/renode.log

python3 tools/check-renode-results.py h743-fdcan \
  build/emu/h743-fdcan/renode.log
~~~

To debug ARM code, open the corresponding compiled ELF with `arm-none-eabi-objdump` or `arm-none-eabi-readelf`. Renode also has an integrated GDB remote server interface. For an on-target fault, pause the emulation, inspect the program counter and stack pointer, examine peripheral registers, and compare the firmware's SRAM status milestones.

For example, direct emulator-side state is at `0x24000000` and its following words. This reserved debugging range exists only in the lab's memory layout, **not** as a standard STM32 hardware status register.

## What this test proves — and what it does not

**Proved on Arch:** actual ARM Cortex-M4 Trampoline firmware executes a periodic task; two emulated STM32H743 Cortex-M7 programs communicate in both directions using a CAN FD frame and a Classical CAN frame, via a modeled M_CAN controller and independent message RAMs.

**Not proved:** physical CAN High/Low voltage signaling, transceiver behavior, wire arbitration timing, precise sample points or data-phase bit rates, a real 500 kbit/s or 2 Mbit/s waveform, power/reset variations, or bus-off recovery under electrical faults. The Renode model logs some nominal/data bit-timing writes as unhandled; therefore this test must **never** be presented as a measured CAN FD bitrate test.

Also, the two exercises are **not yet one integrated AutoAS Basic Software (BSW) plus Trampoline OS firmware**. The STM32H743 register-driver lab is deliberately a minimal peripheral test, **not** an AUTOSAR Microcontroller Abstraction Layer (MCAL). The existing real AutoAS Basic Software (BSW), protocol stack and generated configuration are exercised on Linux in [chapter 11](11-real-autosar-stack.md). Deploying them to these virtual boards still requires target-specific startup, Operating System (OS) mapping, Microcontroller Abstraction Layer (MCAL) compatibility and configuration work.

**The next integration step now works:** [chapter 13](13-real-autoas-mcu.md) links unchanged AutoAS CAN Interface (CanIf), CAN Transport Layer (CanTp), and Protocol Data Unit Router (PduR) code to a Cortex-M7 M_CAN adapter and validates a segmented diagnostic response between two virtual STM32H743 controllers. [Chapter 14](14-real-dcm-mcu.md) now runs the genuine Diagnostic Communication Manager (Dcm) for three verified services on Cortex-M7. Integrating Trampoline Operating System (OS) with this firmware and validating the exact STM32H723/AT32M416 peripherals remain outstanding.

## Reference material

- [Renode CAN hub and machine connections](https://renode.readthedocs.io/en/latest/networking/machine-to-machine-connections.html)
- [Renode CAN and CAN FD host integration](https://renode.readthedocs.io/en/latest/host-integration/can.html)
- [Renode official 1.17.0 release](https://github.com/renode/renode/releases/tag/v1.17.0)
- [ST AN5348 application note for STM32 FDCAN](https://www.st.com/content/st_com/en/technical-documents/an5348-introduction-to-fdcan-peripherals-for-stm32-mcus-stmicroelectronics.html)

**License boundary:** Renode, Trampoline and AutoAS are independent upstream projects with their own licensing. The new freestanding demo code is maintained inside this repository. Neither emulator demonstrates AUTOSAR conformance or functional safety.
