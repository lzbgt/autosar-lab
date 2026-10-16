# 14 — Run the genuine AUTOSAR Diagnostic Communication Manager on virtual STM32

This lab completes another actual **Automotive Open System Architecture (AUTOSAR) Basic Software (BSW)** integration stage: the open-source AutoAS **Diagnostic Communication Manager (Dcm)** is compiled into a real Cortex-M7 firmware image and runs against an emulated STM32H743 CAN controller. It replaces the application-only diagnostic responder from [chapter 13](13-real-autoas-mcu.md).

The demonstration uses **unchanged upstream implementation sources** from pinned AutoAS revision `7bf0210644a932ae45e6cabbe8388729808af7fd`. Only the target driver, configuration tables and fingerprint-data callback are project-owned teaching code.

## Step 1 — Run the real firmware and three diagnostic tests

~~~sh
cd ~/work/autosar-lab
bash tools/run-h743-autoas-dcm.sh
~~~

This command installs the already-pinned Renode emulator if necessary, cross-compiles the real Basic Software modules with `arm-none-eabi-gcc`, and executes three independent Cortex-M7 tester/server pairs on an internal Controller Area Network (CAN) hub. Each run resets the server and tester state.

| Unified Diagnostic Services (UDS) request | Exact verified response | What the real server does |
| --- | --- | --- |
| `22 F1 5B` | `62 F1 5B A5 A5 A5 A5 A5 A5 A5 A5 A5 A5` | ReadDataByIdentifier reads the configured ten-byte fingerprint from an application callback, then sends 13 bytes using ISO 15765-2 Transport Protocol (ISO-TP) segmentation |
| `10 03` | `50 03 00 32 01 F4` | DiagnosticSessionControl validates extended session `0x03`, performs a session transition and returns the configured P2/P2* parameters |
| `22 F1 99` | `7F 22 31` | Rejects the unsupported Data Identifier (DID) with **Negative Response Code (NRC) 0x31**, Request Out Of Range |

The command must print `PASS real AutoAS Dcm` for all three; errors or missing replies cause a nonzero exit code.

The actual emulated server binary lives at `build/emu/h743-autoas-dcm/server.elf`. The three tester variants are the original `build/emu/h743-autoas-tp/tester.elf` and the two additional `tester-session.elf` / `tester-reject.elf` files under `build/emu/h743-autoas-dcm/`.

## Step 2 — Trace each real software module

For every test the incoming data follows:

~~~text
Virtual STM32H743 physical CAN controller model
   │   frame reception from emulated CAN hub (ID 0x731)
   ▼
board_mcan.c                 register/FIFO adapter: project-specific
   │
   ▼
CanIf_RxIndication()         real AutoAS CAN Interface (CanIf)
   │
   ▼
CanTp_RxIndication()         real AutoAS CAN Transport Layer (CanTp)
   │                        removes ISO-TP framing and reassembles PDUs
   ▼
PduR_CanTp* callbacks       real AutoAS Protocol Data Unit Router (PduR)
   │                        delivers a configured diagnostic PDU
   ▼
Dcm_StartOfReception(),     real AutoAS Diagnostic Communication Manager (Dcm)
Dcm_CopyRxData(),
Dcm_MainFunction()
   │
   ▼
Dcm_Dsd.c -> Dcm_Dsp.c      real service dispatch and service logic
   │
   ▼
Demo_ReadFingerPrint()      only the application sensor/data provider
~~~

The outgoing response uses `PduR_DcmTransmit()`, actual `CanTp_Transmit()` and `CanIf_Transmit()`, followed by the microcontroller driver and transmit-confirmation callbacks. On the CAN network, diagnostic tester requests use identifier `0x731` and server replies use `0x732`.

These are the unchanged implementation files compiled from the AutoAS checkout:

| Full module name and abbreviation | Upstream file under `third_party/autoas/infras/` |
| --- | --- |
| CAN Interface (CanIf) | `communication/CanIf/CanIf.c` |
| CAN Transport Layer (CanTp) | `communication/CanTp/CanTp.c` |
| Protocol Data Unit Router (PduR) | `communication/PduR/PduR.c`, `PduR_CanTp.c`, `PduR_Dcm.c` |
| Diagnostic Communication Manager (Dcm), core | `diagnostic/Dcm/Dcm.c` |
| Diagnostic Service Dispatcher (Dsd) | `diagnostic/Dcm/Dcm_Dsd.c` |
| Diagnostic Session Layer (Dsl) | `diagnostic/Dcm/Dcm_Dsl.c` |
| Diagnostic Service Processing (Dsp) | `diagnostic/Dcm/Dcm_Dsp.c` |

Our `dcm_config.c` registers two services, one fingerprint DID (`0xF15B`), two allowed sessions (default and extended), diagnostic-channel buffers and bounded response timing. The `Dcm_Cfg.h` compile-time configuration **does not enable** Diagnostic Trouble Code (DTC) operations, authentication, seed/key security, memory read/write, routine execution, or firmware download.

The CAN hardware adapter at `hardware/simulator/h743-autoas/board_mcan.c` is **not** an AUTOSAR-conformant Microcontroller Abstraction Layer (MCAL) driver. It contains only the registers, FIFO polling, and callbacks needed for this emulation exercise.

## Step 3 — Verify independently observable evidence

The runner reads the diagnostic result from **two different emulated Cortex-M7 CPUs** and checks 20 Static Random-Access Memory (SRAM) words per test, including service-completion status, transmit confirmations, development-error count and raw ISO-TP frame-type counters.

For the 13-byte fingerprint response:

| Observed frame type at each controller's receive FIFO | Tester | Server |
| --- | ---: | ---: |
| Single Frame (SF) | 0 | 1 |
| First Frame (FF) | 1 | 0 |
| Consecutive Frame (CF) | 1 | 0 |
| Flow Control frame (FC) | 0 | 1 |

For the session and negative-response tests, each controller receives one Single Frame (SF), and none of the other three transport-frame types.

The validator also reads separate application and diagnostic counters. The server's session-change counter is **SRAM word 15**; the raw Single Frame counter is word 11, preventing accidental aliasing.

Check the recorded evidence without rerunning the emulator:

~~~sh
python3 tools/check-autoas-dcm.py did \
  build/emu/h743-autoas-dcm/did.log
python3 tools/check-autoas-dcm.py session \
  build/emu/h743-autoas-dcm/session.log
python3 tools/check-autoas-dcm.py reject \
  build/emu/h743-autoas-dcm/reject.log
~~~

Verify the test itself rejects corrupted evidence:

~~~sh
python3 tools/check-autoas-dcm.py --self-test
~~~

That self-test deliberately corrupts response status, received frame counts, transmit confirmations and development-error counters and requires the validator to fail each corrupted case.

## Step 4 — Compile and debug individual modules

~~~sh
bash tools/build-h743-autoas-dcm.sh

arm-none-eabi-nm build/emu/h743-autoas-dcm/server.elf |
  grep -E 'Dcm_|PduR_|CanTp_|CanIf_'

arm-none-eabi-size build/emu/h743-autoas-dcm/server.elf
~~~

The actual `Dcm_DspReadDataByIdentifier()` determines whether `F1 5B` is allowed, calls the configured fingerprint provider, and constructs the service response. `Dcm_DspSessionControl()` validates `0x03` and generates the positive timing response. Unsupported `F1 99` generates the negative response inside the real service code.

With Renode's GDB integration, useful breakpoints are `Dcm_MainFunction()`, `Dsd_HandleRequest()`, `Dcm_DspReadDataByIdentifier()` and `Dcm_DspSessionControl()`. Some helpers are internal static functions, so compile with symbols (`-g3`) and inspect source or symbols directly.

## Step 5 — Verify actual Cortex-M7 SysTick timing

The emulator server now has a separate `startup-systick.S` exception vector, so a real Cortex-M7 **SysTick interrupt** drives a 1 millisecond counter. `CanTp_MainFunction_Fast()` runs in main context once per elapsed tick, while `CanTp_MainFunction()` and `Dcm_MainFunction()` run every tenth tick. The interrupt handler itself only updates a counter, avoiding Basic Software (BSW) service reentrancy.

Renode's STM32H743 starts with a **64 MHz reset-clock configuration** rather than the maximum configured 480 MHz clock. Initially setting the SysTick reload for 480 MHz produced only 159 interrupts in 1.2 seconds, exposing the clock mismatch. Using `LOAD=63999` (=64 MHz /1000 − 1) produced the measured timing:

| Event over 1.2 seconds of simulated time | Observed count |
| --- | ---: |
| Emulated SysTick 1 ms interrupts | 1,200 |
| Real CAN Transport Layer (CanTp) fast service executions | 1,199 |
| CAN Transport Layer (CanTp) and Diagnostic Communication Manager (Dcm) 10 ms main executions | 119 |

The Renode scenario samples these counts directly from server Static Random-Access Memory (SRAM) words 16–18 at addresses `0x24000040`, `0x24000044` and `0x24000048`. The regression fails if the SysTick frequency, fast-service count or ten-to-one main-service ratio is wrong. All three diagnostic transactions continue to pass.

These are **verified virtual-time scheduling counts**, not measured processor execution deadlines on physical STM32H723 hardware. They do not by themselves validate worst-case execution times, bus-off fault recovery or deployment on Trampoline.

## Step 6 — Know exactly what remains unverified

The successful end-to-end experiment proves that **real embedded AutoAS CAN Interface (CanIf), CAN Transport Layer (CanTp), Protocol Data Unit Router (PduR) and Diagnostic Communication Manager (Dcm) functionality** can run together on Cortex-M7 and exchange diagnostic messages using an emulated M_CAN peripheral.

It does **not** prove AUTOSAR certification, correct hardware bit timing, physical transceiver behavior, real STM32H723 peripheral clocks and memory layout, or AT32M416 compatibility. The simulator models an STM32H743 rather than the specific hardware you own. The emulated driver does not implement complete bus-off recovery, transmit queue management, interrupts and timeout supervision.

There is still **no single firmware image integrating Trampoline Operating System (OS), a generated Runtime Environment (RTE), and all these Basic Software modules**. The actual 1 ms/10 ms emulated timer cadence is now verified, but task priorities, interrupt interference, worst-case execution time and broader timeout/fault-injection coverage remain outstanding. The specific missing-Flow-Control timeout and subsequent same-server recovery are now verified in [chapter 15](15-iso-tp-fault-recovery.md).

The next useful engineering step is to integrate Trampoline operating-system tasks with these proven diagnostic modules, extend the fault tests to bus-off and recovery policies, and then port the hardware driver to STM32H723 and AT32M416 for physical bench verification.

**Licensing:** The pinned AutoAS source is GPL version 3/commercial dual-licensed. The upstream checkout remains separate, unchanged and Git-ignored. Renode and Trampoline have their own licenses.
