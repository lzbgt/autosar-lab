# AUTOSAR Lab: from virtual ECUs to real CAN FD hardware

A free, **hands-on learning project** for AUTOSAR Classic concepts, ECU communication, diagnostic services and embedded integration. Start on Linux without a board, then move the portable application/communication core toward STM32H723 and AT32M416.

> **Scope:** The included teaching ECU is **not an AUTOSAR implementation, a certified BSW stack, a generated RTE, a safety system or production firmware**. Modules intentionally resemble Classic communication paths to make tracing and experimentation easy. For authentic OS/configuration and BSW exercises use independently licensed upstream software in `docs/06-upstream-stacks.md`.

## Fast start (no root required)

```sh
cd ~/work/autosar-lab
bash tools/run-all.sh
```

To execute the real Trampoline OS POSIX periodic-task example (verified on Arch):

```sh
bash tools/run-trampoline-posix.sh
```

For SocketCAN `vcan0`, first provision a working kernel module/interface using `tools/setup-vcan.sh` and then:

```sh
python3 tools/smoke.py --backend socketcan --ifname vcan0 --build-dir build
```

The UDP virtual-bus backend is an unprivileged **frame-delivery simulator**, NOT an electrical CAN/controller emulator: it does not model arbitration, error frames, ACK or bit timing. SocketCAN `vcan` is also not hardware-level simulation. Both are for application/protocol learning.

## Repository map

- `docs/` — numbered beginner guide; architecture, SWC/RTE/BSW/MCAL, CAN/CAN FD, UDS and ISO-TP, build/debug/deployment, Trampoline OS, real boards, glossary and pitfalls.
- `src/core/` — portable C11 mini-ECU teaching model. Application runnable, RTE-like call, COM-like signal packing, a narrow PduR/CanIf-like TX path, DCM-like UDS handler and limited ISO-TP response sender.
- `src/host/`, `src/platform/` — Linux ECU process and selectable real SocketCAN/UDP bus adapter.
- `src/tests/` — deterministic no-root component tests.
- `tools/` — rootless virtual bus, UDS tester, smoke tests, vcan helper, ARM compile-only check, third-party installer.
- `hardware/` — STM32H723 and AT32M416 CAN FD bring-up guides and port contracts.
- `config/` — documented CAN IDs and signals used in the examples.
- `examples/` — guided upstream RTOS/BSW labs, intentionally isolated from the local teaching stack.

## Lab sequence

| Lab | What you verify | Runs now? |
| --- | --- | --- |
| L0 | CMake build, CTest, GNU ARM compile-only | Yes, no root |
| L1 | Two virtual participants on local UDP bus, speed frames | Yes, no root |
| L2 | UDS 0x10/0x22, single/multi frame ISO-TP replies and negative responses | Yes, no root |
| L3 | Same exercises over SocketCAN vcan | Requires `vcan` kernel support |
| L4 | Real AUTOSAR OS-aligned tasks and alarms with Trampoline POSIX | Separate optional upstream build |
| L5 | Open-source BSW module exploration (AutoAS) | Separate optional upstream build; platform-specific |
| L6 | H723 + AT32M416 Classical CAN then CAN FD | Needs boards, transceivers and vendor HAL ports |

Begin with [docs/00-learning-path.md](docs/00-learning-path.md). Follow the actual source code, not just the diagrams.

## Reproducibility and security

Owned example code uses the MIT license (`LICENSE`). Upstream AUTOSAR documents and Trampoline/AutoAS retain **their own** licenses; do not copy or redistribute vendor SDKs or proprietary MCAL code in this repository. No secrets, firmware images or generated build files are committed. Use a short, isolated CAN bus and non-actuating demonstrations until hardware behavior has been measured.
