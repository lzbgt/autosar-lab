# 00 — Learning path: beginner to physical Electronic Control Unit (ECU)

Read these documents in order. The goal is **understanding and reproducing a system**, not memorizing names of AUTOSAR modules.

## Who this is for and what you need

- Experience in C is useful but not mandatory for the first protocol labs.
- Required on Linux: GCC, CMake, Ninja (or Make), Python 3, Git. ARM GCC is only for Stage 3.
- No automotive hardware, paid vendor toolchain, proprietary licenses or internet connection is needed for Labs 0–3.
- Optional physical stage: STM32H723, AT32M416CBT7, two Controller Area Network Flexible Data-Rate (CAN FD)-compatible transceiver modules, 2 x 120-ohm terminations, common ground, regulated power, Serial Wire Debug (SWD) probe.
- Do not connect this lab to a vehicle or real actuator network.

## The essential distinction

**AUTOSAR is both a software architecture and a configuration/integration methodology.** The architecture defines interfaces/layers; commercial toolchains ingest standardized descriptions and generate parts of Runtime Environment (RTE)/Basic Software (BSW) configuration. Calling a C program with function names that resemble AUTOSAR does **not** make it AUTOSAR conformant.

We use three independent tracks:

1. **Concept emulator** (this repository): small, transparent, C11, compiles offline; teaches signal flow, Controller Area Network (CAN), simplified ISO 15765-2 Transport Protocol (ISO-TP) and Unified Diagnostic Services (UDS).
2. **Authentic Operating System (OS) and Basic Software (BSW) study**: Trampoline for OSEK/AUTOSAR-Operating System (OS)-like tasks/configuration, AutoAS for Basic Software (BSW) reading and platform-dependent simulator experiments. These are separate projects with their own licenses/limitations.
3. **Microcontroller implementation**: reuse the testable application logic; integrate actual vendor drivers, a real scheduler/Operating System (OS) and (when available) AUTOSAR Basic Software (BSW)/Microcontroller Abstraction Layer (MCAL).

## Exercises and exit criteria

| Stage | What to do | Evidence required |
| --- | --- | --- |
| L0 (30–60 min) | Build host C tests; examine 10 ms runnable and 100 ms AUTOSAR Communication module (Com) transmit | CTest green, know why speed=50 cm/s at 100 ms |
| L1 (1–2 h) | Run rootless UDP bus and Electronic Control Unit (ECU); inspect Controller Area Network (CAN) 0x201 frames | Understand ID, payload, cycle time, signal scaling |
| L2 (2–4 h) | Unified Diagnostic Services (UDS) session 0x10; Data Identifier (DID) 0xF187 and 0xF190; unsupported Data Identifier (DID) | Explain Single Frame (SF), First Frame (FF), Flow Control frame (FC), Consecutive Frame (CF) and Negative Response Code (NRC) 0x31 |
| L3 (1–2 h) | Enable FD payload mode; if available, repeat with SocketCAN vcan | Distinguish FD data phase / Data Length Code (DLC) from simulated UDP |
| L4 (half-day) | Run Trampoline Portable Operating System Interface (POSIX) example with tasks/alarms; read OSEK Implementation Language (OIL) or AUTOSAR XML (ARXML) | Identify task, alarm, counter, GOIL-produced config |
| L5 (half-day) | Trace real Basic Software (BSW) in AutoAS and its documented diagnostic demo | Draw Diagnostic Communication Manager (Dcm) ⇄ Protocol Data Unit Router (PduR) ⇄ CAN Transport Layer (CanTp) ⇄ CAN Interface (CanIf) ⇄ Can |
| L6 (emulated MCU) | Run the real Trampoline Cortex-M4 OS on virtual STM32F407 and real Cortex-M7 register-level CAN FD sender/receiver firmware on two virtual STM32H743s | GPIO D15 task transitions and paired `0xCAFEFD10` / `0xCAFECC08` diagnostic memory markers; no physical bit-rate claim |
| L6.1 (real embedded Basic Software) | Compile AutoAS CAN Interface (CanIf), CAN Transport Layer (CanTp), and Protocol Data Unit Router (PduR) from genuine upstream code for two virtual STM32H743 Cortex-M7 processors | CAN FD and Classical CAN routing; real transport segmentation/reassembly; both hardware SRAM success markers and no development-error reports |
| L7 (1–2 days) | Build vendor Hardware Abstraction Layer (HAL) Controller Area Network (CAN) 2.0 on H723 and M416, observe packets | Verified two-node communication at 500 kbit/s |
| L8 (1–2 days) | Enable ISO Controller Area Network Flexible Data-Rate (CAN FD) 500 kbit/s nominal + 2 Mbit/s data | Frame trace + error counters + handling tests |
| L9 (advanced) | Integrate Operating System (OS), configuration, Microcontroller Abstraction Layer (MCAL) and Basic Software (BSW) on Microcontroller Unit (MCU) | Real task scheduling, memory map and diagnostic timing metrics |

Times are suggested study effort, not promises of automatic completion.

## Daily order of reading

1. `01-architecture.md` — layers, Virtual Functional Bus (VFB), Electronic Control Unit (ECU) scope, responsibility boundaries.
2. `02-os-rte-components.md` — Software Components (SWCs), ports, runnables, task scheduling and Runtime Environment (RTE) configuration.
3. `03-can-uds-isotp.md` — physical framing and diagnostics, with byte-level calculations.
4. `04-virtual-labs.md` — instructions for the runnable host code.
5. `05-toolchain-debugging.md` — build, debug, trace and reproducibility.
6. `06-upstream-stacks.md` — authentic Trampoline/AutoAS and limitations.
7. `07-mcu-bringup.md` — two real MCUs, Controller Area Network Flexible Data-Rate (CAN FD) transceivers, flash/debug.
8. `08-conformance-and-next-steps.md` — production distinctions and realistic extensions.
9. `09-glossary-and-references.md` — definitions and official reading.
10. `10-troubleshooting.md` — common failures and a debug decision tree.
11. `11-real-autosar-stack.md` — compile real AutoAS Basic Software (BSW), run its configuration generators, and verify actual Unified Diagnostic Services (UDS) transport.
12. `12-mcu-simulation.md` — install Renode, compile genuine Cortex-M4 Trampoline firmware, exchange Controller Area Network Flexible Data-Rate (CAN FD) frames through two emulated Cortex-M7 controllers, and verify real peripheral registers.
13. `13-real-autoas-mcu.md` — execute actual AutoAS Basic Software (BSW) CAN Interface (CanIf), CAN Transport Layer (CanTp), and Protocol Data Unit Router (PduR) in Cortex-M7 firmware with real configured routing and a segmented diagnostic transfer.

## How to practice effectively

For each lesson, **predict** a frame/state transition first, execute it, compare with trace, then change one parameter and rerun. Write a short note: input, expected result, actual result, cause of any difference. Each guide specifies a concrete observable result.

Recommended first command:

~~~sh
cd ~/work/autosar-lab
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 tools/smoke.py --backend udp --build-dir build
~~~
