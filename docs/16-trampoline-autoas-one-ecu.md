# 16 — One STM32H743 firmware: Trampoline operating system + AutoAS diagnostics

This lab combines two **real upstream software implementations** in **one executable for an ARM Cortex-M7 processor**:

- **Trampoline Operating System (OS):** an OSEK/AUTOSAR-aligned real-time kernel, with application tasks and alarms configured using OSEK Implementation Language (OIL). A generated interrupt handler activates a real task each millisecond.
- **AutoAS Basic Software (BSW):** the actual CAN Interface (CanIf), CAN Transport Layer (CanTp), Protocol Data Unit Router (PduR), and Diagnostic Communication Manager (Dcm) implementations. The same unchanged source modules are used in [the standalone diagnostic exercise](14-real-dcm-mcu.md).

These run within **one emulated STM32H743 Cortex-M7**, sharing the same memory map, interrupt controller and CAN controller. A **second independent emulated STM32H743** acts as the diagnostic tester. Neither Trampoline nor AutoAS is replaced by a mock implementation.

## 1. Build and run all three services

~~~sh
cd ~/work/autosar-lab
bash tools/run-h743-trampoline-autoas.sh
~~~

The runner downloads no commercial software. It uses the pinned open-source Trampoline and AutoAS checkouts in the Git-ignored `third_party/` folder, the ARM GNU Compiler Collection (`arm-none-eabi-gcc`), the real Trampoline OIL configuration compiler (GOIL), and the project-local Renode emulator.

Expected results from three separate test runs:

| Tester request | Actual verified server response | Observed processing |
| --- | --- | --- |
| `22 F1 5B` | `62 F1 5B A5 A5 A5 A5 A5 A5 A5 A5 A5 A5` | ReadDataByIdentifier using the real Diagnostic Communication Manager (Dcm) and a segmented 13-byte ISO Transport Protocol (ISO-TP) response |
| `10 03` | `50 03 00 32 01 F4` | DiagnosticSessionControl switches to extended session |
| `22 F1 99` | `7F 22 31` | Unsupported Data Identifier (DID) produces Negative Response Code (NRC) `0x31` |

All response payloads are verified by another genuine AutoAS communication stack running on the independent tester.

The server executable is generated under `build/emu/h743-trampoline-autoas/trampoline_autoas.elf`, and its GOIL-generated sources remain under `build/emu/h743-trampoline-autoas/trampoline_autoas/`. Large third-party source checkouts, generated files and executable binaries are **not committed** to the learning repository.

## 2. OIL configuration → generated OS task → genuine diagnostic work

Read the tracked configuration directly at `hardware/simulator/h743-trampoline/trampoline_autoas.oil`.

Its relevant configuration is:

~~~oil
TASK bsw_tick {
  PRIORITY = 1;
  AUTOSTART = FALSE;
  ACTIVATION = 1;
  SCHEDULE = FULL;
};
ALARM bsw_periodic {
  COUNTER = SystemCounter;
  ACTION = ACTIVATETASK { TASK = bsw_tick; };
  AUTOSTART = TRUE {
    APPMODE = std;
    ALARMTIME = 1;
    CYCLETIME = 1;
  };
};
~~~

The alarm activates one Trampoline task every **1 millisecond**. The Cortex-M7 SysTick exception enters the **GOIL-generated Trampoline handler**, which updates the OS counter and schedules the task. The task invokes the following shared Basic Software service interface:

~~~c
TASK(bsw_tick) {
    AutoAS_BswPoll();
    AutoAS_BswTick1ms();
    AutoAS_BswUpdate();
    TerminateTask();
}
~~~

The three application functions in `hardware/simulator/h743-autoas-dcm/dcm_server.c` perform real work:

| Invocation | Input | Real module behavior |
| --- | --- | --- |
| `AutoAS_BswPoll()` | CAN controller receive FIFO, transmit-completion flag | Receives CAN frames through the minimal target-specific M_CAN adapter, forwards them to real AutoAS CAN Interface callbacks |
| `AutoAS_BswTick1ms()` | Each real Trampoline task activation | Executes actual `CanTp_MainFunction_Fast()`, and every tenth tick calls `CanTp_MainFunction()` and `Dcm_MainFunction()` |
| `AutoAS_BswUpdate()` | Actual Diagnostic Communication Manager (Dcm) context, data callback and CAN transmit confirmations | Writes test evidence only when configured diagnostics and transport have genuinely completed |

Initialization `AutoAS_BswInit()` runs once before `StartOS()` to configure genuine CAN Interface (CanIf), CAN Transport Layer (CanTp), Protocol Data Unit Router (PduR), Diagnostic Communication Manager (Dcm), and the CAN controller. Only Trampoline owns and configures SysTick in this firmware; there is **no second OS-simulating timer loop**.

A **single Trampoline task** performs the services in sequence. This is deliberate: the demonstration has not established that the different AutoAS services are reentrant across preempting tasks. Independent 1 ms and 10 ms task priorities, deadline analysis, and shared-state protection can be introduced after an explicit concurrency review.

## 3. Exact built software and hardware boundary

The builder `tools/build-h743-trampoline-autoas.sh` does not substitute generated functions for the real kernel. It:

1. Verifies the pinned upstream Trampoline source revision and compiles actual AutoAS source.
2. Invokes the genuine Trampoline GOIL generator on `trampoline_autoas.oil`. The nearest upstream-supported ARMv7E-M configuration schema is `cortex-m/armv7em/stm32f407/stm32f4discovery`.
3. Compiles the actual Trampoline scheduler, task/kernel/alarm management, Cortex-M context-switch assembly, and generated interrupt dispatch with `-mcpu=cortex-m7`.
4. Supplies a small, project-owned **STM32H743 board port** at `hardware/simulator/h743-trampoline/board_port.c`: memory startup, clock configuration and SysTick initialization for the emulator. It deliberately excludes the upstream STM32F407 Reset and peripheral driver implementations.
5. Compiles the genuine AutoAS communication and diagnostic modules into the **same ARM Executable and Linkable Format (ELF)**, using `autoas_bsw.h` for the small shared application interface.

The firmware's memory and interrupt layout is in `hardware/simulator/h743-trampoline/memory.ld`. It uses flash at `0x08000000` and main SRAM at `0x24001000`, leaving diagnostic-observation SRAM at `0x24000000`.

**Important C type collision:** Trampoline and AutoAS have different definitions of their `ComStack_Types.h`, including `PduIdType` (8-bit in the pinned Trampoline communication header, 16-bit in AutoAS). They must not be included within the same C translation unit. The build uses **separate include lists for the two upstream implementations**, and the integration bridge exports only `void` service functions. This avoids silently rewriting or misrepresenting either library's protocol types.

**The presence of Trampoline OS does not mean a generated Runtime Environment (RTE) is linked.** The OIL task calls application-side service functions directly. The separate configuration-generation exercise in [chapter 11](11-real-autosar-stack.md) remains separate.

## 4. Follow a diagnostic request through a scheduled task

For the fingerprint read `22 F1 5B`, the actual route is:

~~~text
Tester Cortex-M7 ─── CAN 0x731 ───▶ Server Cortex-M7
                                    │
                                M_CAN RX FIFO
                                    │
                 real Trampoline TASK(bsw_tick), every 1 ms
                                    │
                            CanIf_RxIndication
                                    │
                              CanTp reassembly
                                    │
                              PduR route table
                                    │
                            Dcm service dispatch
                                    │
                    Demo_ReadFingerPrint: ten 0xA5 bytes
                                    │
                      PduR -> CanTp -> CanIf -> M_CAN
                                    │
Tester Cortex-M7 ◀── CAN 0x732 ────┘
~~~

The real server transmits the **First Frame (FF)** with six response bytes. The tester returns a **Flow Control frame (FC)**. The server then transmits a **Consecutive Frame (CF)** with the remaining seven bytes. All three frame types, the reconstructed service payload, and both endpoints' transmit confirmations are independently checked.

## 5. Observe Trampoline timing and diagnostic state

The runner samples actual memory from both emulated processors. In a typical successful 1.22-second virtual-time run (20 ms for server initialization followed by 1.20 s of diagnostic exchange):

| Memory evidence on server | Observed |
| --- | ---: |
| Trampoline OS task activations (SRAM word 25) | 1,219 |
| Ten-millisecond application task intervals (word 26) | 121 |
| Actual AutoAS one-millisecond transport executions (word 17) | 1,219 |
| Actual AutoAS ten-millisecond diagnostic executions (word 18) | 121 |
| Development Error Tracer (DET) error count (word 9) | 0 |
| Successful 13-byte fingerprint result | `0xCAFE22DC` |

These are counters from the **same loaded ARM ELF**; actual service callbacks and transport frame counters must also pass. Another test does not qualify as successful by only seeing an OS task count or an emulator exit code.

Run only the result-checker's deliberate corruption tests:

~~~sh
python3 tools/test-h743-trampoline-autoas.py --self-test
~~~

The acceptance checks reject false service success, missing real-time OS task activity, wrong 10 ms cadence, mismatched Operating System (OS) and Basic Software (BSW) execution counts and unexpected diagnostic errors.

**Startup ordering is important:** the real tester must transmit only after the server has started its Trampoline OS and configured its CAN controller. The Renode runner explicitly boots the server **20 ms before the tester**. Starting both simultaneously previously lost the first request before the server configured its CAN receive interface. This test uses a fixed initialization lead time; a production design should use an explicit controller-ready handshake rather than assuming all targets initialize in 20 ms.

## 6. What remains outstanding

This demonstration establishes **actual integrated execution** of Trampoline and AutoAS in one emulated Cortex-M7 firmware. It does not establish full AUTOSAR Classic Platform conformance, a standard Runtime Environment (RTE) connection, Microcontroller Abstraction Layer (MCAL) completeness, hardware execution deadlines, priority-inversion behavior, ISO-TP retransmission timing in all error cases, real CAN arbitration, physical transceivers or compatibility with your exact STM32H723 and AT32M416.

The Trampoline task and timer run under Renode's STM32H743 model, whose reset-time CPU clock is assumed to be 64 MHz. Actual MCU clock configuration and worst-case service execution time still need bench validation.

**Critical license boundary:** the pinned Trampoline kernel declares GNU General Public License version 2 terms in its kernel source, while AutoAS is GNU General Public License version 3/commercial dual-licensed. Combining and **distributing** their code as a single firmware binary may require compatible licensing permissions and legal review; do not assume GPLv2-only and GPLv3 are automatically compatible. This repository distributes the independent *build instructions and project-owned source*—not the compiled combined binary or either upstream checkout. A commercial AutoAS license alone does not automatically resolve Trampoline's separate terms.

For physical validation, first verify vendor-specific memory/clock/peripheral integration and compatible licensing, then use a properly terminated two-node CAN Flexible Data-Rate (CAN FD) bench network and independent analyzer. Continue with [board bring-up](07-mcu-bringup.md) once the exact firmware ports are ready.
