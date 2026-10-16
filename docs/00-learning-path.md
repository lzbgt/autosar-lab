# 00 — Learning path: beginner to physical ECU

Read these documents in order. The goal is **understanding and reproducing a system**, not memorizing names of AUTOSAR modules.

## Who this is for and what you need

- Experience in C is useful but not mandatory for the first protocol labs.
- Required on Linux: GCC, CMake, Ninja (or Make), Python 3, Git. ARM GCC is only for Stage 3.
- No automotive hardware, paid vendor toolchain, proprietary licenses or internet connection is needed for Labs 0–3.
- Optional physical stage: STM32H723, AT32M416CBT7, two CAN FD-compatible transceiver modules, 2 x 120-ohm terminations, common ground, regulated power, SWD probe.
- Do not connect this lab to a vehicle or real actuator network.

## The essential distinction

**AUTOSAR is both a software architecture and a configuration/integration methodology.** The architecture defines interfaces/layers; commercial toolchains ingest standardized descriptions and generate parts of RTE/BSW configuration. Calling a C program with function names that resemble AUTOSAR does **not** make it AUTOSAR conformant.

We use three independent tracks:

1. **Concept emulator** (this repository): small, transparent, C11, compiles offline; teaches signal flow, CAN, simplified ISO-TP and UDS.
2. **Authentic OS and BSW study**: Trampoline for OSEK/AUTOSAR-OS-like tasks/configuration, AutoAS for BSW reading and platform-dependent simulator experiments. These are separate projects with their own licenses/limitations.
3. **Microcontroller implementation**: reuse the testable application logic; integrate actual vendor drivers, a real scheduler/OS and (when available) AUTOSAR BSW/MCAL.

## Exercises and exit criteria

| Stage | What to do | Evidence required |
| --- | --- | --- |
| L0 (30–60 min) | Build host C tests; examine 10 ms runnable and 100 ms COM transmit | CTest green, know why speed=50 cm/s at 100 ms |
| L1 (1–2 h) | Run rootless UDP bus and ECU; inspect CAN 0x201 frames | Understand ID, payload, cycle time, signal scaling |
| L2 (2–4 h) | UDS session 0x10; DID 0xF187 and 0xF190; unsupported DID | Explain SF, FF, FC, CF and NRC 0x31 |
| L3 (1–2 h) | Enable FD payload mode; if available, repeat with SocketCAN vcan | Distinguish FD data phase / DLC from simulated UDP |
| L4 (half-day) | Run Trampoline POSIX example with tasks/alarms; read OIL or ARXML | Identify task, alarm, counter, GOIL-generated config |
| L5 (half-day) | Trace real BSW in AutoAS and its documented diagnostic demo | Draw Dcm ⇄ PduR ⇄ CanTp ⇄ CanIf ⇄ Can |
| L6 (1–2 days) | Build vendor HAL CAN 2.0 on H723 and M416, observe packets | Verified two-node communication at 500 kbit/s |
| L7 (1–2 days) | Enable ISO CAN FD 500 kbit/s nominal + 2 Mbit/s data | Frame trace + error counters + handling tests |
| L8 (advanced) | Integrate OS, configuration, MCAL and BSW on MCU | Real task scheduling, memory map and diagnostic timing metrics |

Times are suggested study effort, not promises of automatic completion.

## Daily order of reading

1. `01-architecture.md` — layers, VFB, ECU scope, responsibility boundaries.
2. `02-os-rte-components.md` — SWCs, ports, runnables, task scheduling and RTE configuration.
3. `03-can-uds-isotp.md` — physical framing and diagnostics, with byte-level calculations.
4. `04-virtual-labs.md` — instructions for the runnable host code.
5. `05-toolchain-debugging.md` — build, debug, trace and reproducibility.
6. `06-upstream-stacks.md` — authentic Trampoline/AutoAS and limitations.
7. `07-mcu-bringup.md` — two real MCUs, CAN FD transceivers, flash/debug.
8. `08-conformance-and-next-steps.md` — production distinctions and realistic extensions.
9. `09-glossary-and-references.md` — definitions and official reading.
10. `10-troubleshooting.md` — common failures and a debug decision tree.

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
