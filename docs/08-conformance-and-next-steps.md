# 08 — From a teaching prototype to a real AUTOSAR ECU

## Reality check

This repository demonstrates architecture-shaped **application and protocol flows**, not AUTOSAR SWS API conformance. Important things it deliberately omits:

- ARXML descriptions, ECU extract, full ECUC configuration and actual RTE generation.
- Memory mapping (MemMap), BSW module init/lifecycle sequencing (EcuM/BswM), OS scheduling protections.
- COM signal update bit handling, gateway PDU routing tables, CanIf controller state management, CanSM/ComM/Nm.
- Full CanTp state machine (multiple concurrent channels, incoming multi-frame requests, timeout profiles, addressing variants), full Dcm service and NRC behavior.
- Watchdog integration, Dem events/DTCs, NvM blocks, error detection reporting, safety mechanisms.
- Security access, authentication, software update/flashloader, DoIP, secure boot.
- Configuration validation, conformance and interoperability tests required by actual OEM integration.

The demo uses one ECU, a bounded sender state, Python tester, one telemetry frame and narrow UDS services. It assumes an isolated lab network. The **portable application** is a reusable exercise; the mini BSW is a teaching aid.

## How to design the genuine staged project

1. **OS**: Trampoline POSIX OIL/GOIL generated alarm/task exercise. Measure task releases.
2. **SWC/RTE**: build ARXML SWCs/ports/runnables, generate source using a tool that actually supports your chosen AUTOSAR release. Inspect RTE API and data mappings.
3. **Communication BSW**: integrate an implementation covering Com/PduR/CanIf/CanTp/Dcm; validate Can IDs, PDU IDs and buffer alignment.
4. **MCAL**: select supported H723/M416 hardware drivers, clock source, CAN controller init, filters and ISR/task boundary. Custom driver ≠ AUTOSAR MCAL certification.
5. **ECU startup**: configure EcuM/BswM module init order, OS task mapping, watchdog and diagnostics state.
6. **Hardware-in-loop**: CAN FD analyzer, independent tester, fault injections, thermal and supply variation.
7. **Production engineering**: trace requirements to tests, licensing, ISO 26262 where relevant, ISO/SAE 21434 for cybersecurity, audit and update strategy.

## What to compare in an interview or integration review

| Question | Correct target |
| --- | --- |
| What is generated vs handwritten? | SWC logic typically handwritten; RTE/config code generated; OS/MCAL supplied & configured |
| What is CAN FD's improvement? | Up to 64 payload bytes and optionally faster data phase, not a higher priority arbitration ID |
| What is CanTp? | Transport segmentation/reassembly and flow control, distinct from COM signals |
| Dcm vs Dem? | Diagnostic service handling versus event/DTC storage and status management |
| What is VFB? | System-level logical SWC communication abstraction |
| Why is MCAL necessary? | Hardware-dependent peripheral drivers behind standardized API/configuration |
| What proves integration? | Build plus measurable interface, timing, error/recovery and HIL evidence |

## Suggested capstone

Build a **two-ECU sensor gateway**:
- Sensor ECU (M416) samples an analog input and publishes speed at 100Hz/10ms.
- Main ECU (H723) filters the signal in a periodic SWC, publishes a processed signal and services UDS DIDs/diagnostics.
- The host captures bus traces and correlates TX/RX timing, dropped frames and MCU UART/SWO logs.

Start read-only with a dummy sensor and no motor actuation. Once real Classic drivers and task/event configuration work, add robust memory/diagnostic/error management.

## Scope of verification

At each milestone, record: exact sources/commit, compiler version, board firmware hashes, hardware revisions, wiring, test commands, raw observations and known limitations. A screenshot or "it seems to work" is weaker than deterministic test/trace with counts and timestamps.
