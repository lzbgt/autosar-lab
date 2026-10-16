# 08 — From a teaching prototype to a real AUTOSAR Electronic Control Unit (ECU)

## Reality check

This repository demonstrates architecture-shaped **application and protocol flows**, not AUTOSAR SWS Application Programming Interface (API) conformance. Important things it deliberately omits:

- AUTOSAR XML (ARXML) descriptions, Electronic Control Unit (ECU) extract, full Electronic Control Unit Configuration (ECUC) configuration and actual Runtime Environment (RTE) generation.
- Memory mapping (MemMap), Basic Software (BSW) module init/lifecycle sequencing (Electronic Control Unit State Manager (EcuM)/Basic Software Mode Manager (BswM)), Operating System (OS) scheduling protections.
- AUTOSAR Communication module (Com) signal update bit handling, gateway Protocol Data Unit (PDU) routing tables, CAN Interface (CanIf) controller state management, CAN State Manager (CanSM)/Communication Manager (ComM)/Nm.
- Full CAN Transport Layer (CanTp) state machine (multiple concurrent channels, incoming multi-frame requests, timeout profiles, addressing variants), full Diagnostic Communication Manager (Dcm) service and Negative Response Code (NRC) behavior.
- Watchdog integration, Diagnostic Event Manager (Dem) events/Diagnostic Trouble Codes (DTCs), Nonvolatile Memory Manager (NvM) blocks, error detection reporting, safety mechanisms.
- Security access, authentication, software update/flashloader, DoIP, secure boot.
- Configuration validation, conformance and interoperability tests required by actual OEM integration.

The demo uses one Electronic Control Unit (ECU), a bounded sender state, Python tester, one telemetry frame and narrow Unified Diagnostic Services (UDS) services. It assumes an isolated lab network. The **portable application** is a reusable exercise; the mini Basic Software (BSW) is a teaching aid.

## How to design the genuine staged project

1. **Operating System (OS)**: Trampoline Portable Operating System Interface (POSIX) OSEK Implementation Language (OIL)/Trampoline configuration generator (GOIL) generated alarm/task exercise. Measure task releases.
2. **Software Component (SWC)/Runtime Environment (RTE)**: build AUTOSAR XML (ARXML) Software Components (SWCs)/ports/runnables, generate source using a tool that actually supports your chosen AUTOSAR release. Inspect Runtime Environment (RTE) Application Programming Interface (API) and data mappings.
3. **Communication Basic Software (BSW)**: integrate an implementation covering Communication module (Com)/Protocol Data Unit Router (PduR)/CAN Interface (CanIf)/CAN Transport Layer (CanTp)/Diagnostic Communication Manager (Dcm); validate Can IDs, Protocol Data Unit (PDU) IDs and buffer alignment.
4. **Microcontroller Abstraction Layer (MCAL)**: select supported H723/M416 hardware drivers, clock source, Controller Area Network (CAN) controller init, filters and ISR/task boundary. Custom driver ≠ AUTOSAR Microcontroller Abstraction Layer (MCAL) certification.
5. **Electronic Control Unit (ECU) startup**: configure Electronic Control Unit State Manager (EcuM)/Basic Software Mode Manager (BswM) module init order, Operating System (OS) task mapping, watchdog and diagnostics state.
6. **Hardware-in-loop**: Controller Area Network Flexible Data-Rate (CAN FD) analyzer, independent tester, fault injections, thermal and supply variation.
7. **Production engineering**: trace requirements to tests, licensing, ISO 26262 where relevant, ISO/SAE 21434 for cybersecurity, audit and update strategy.

## What to compare in an interview or integration review

| Question | Correct target |
| --- | --- |
| What is generated vs handwritten? | Software Component (SWC) logic typically handwritten; Runtime Environment (RTE)/config code generated; Operating System (OS)/Microcontroller Abstraction Layer (MCAL) supplied & configured |
| What is Controller Area Network Flexible Data-Rate (CAN FD)'s improvement? | Up to 64 payload bytes and optionally faster data phase, not a higher priority arbitration ID |
| What is CAN Transport Layer (CanTp)? | Transport segmentation/reassembly and flow control, distinct from AUTOSAR Communication module (Com) signals |
| Diagnostic Communication Manager (Dcm) vs Diagnostic Event Manager (Dem)? | Diagnostic service handling versus event/Diagnostic Trouble Code (DTC) storage and status management |
| What is Virtual Functional Bus (VFB)? | System-level logical Software Component (SWC) communication abstraction |
| Why is Microcontroller Abstraction Layer (MCAL) necessary? | Hardware-dependent peripheral drivers behind standardized Application Programming Interface (API)/configuration |
| What proves integration? | Build plus measurable interface, timing, error/recovery and Hardware-in-the-Loop (HIL) evidence |

## Suggested capstone

Build a **two-Electronic Control Unit (ECU) sensor gateway**:
- Sensor Electronic Control Unit (ECU) (M416) samples an analog input and publishes speed at 100Hz/10ms.
- Main Electronic Control Unit (ECU) (H723) filters the signal in a periodic Software Component (SWC), publishes a processed signal and services Unified Diagnostic Services (UDS) Data Identifiers (DIDs)/diagnostics.
- The host captures bus traces and correlates Transmit (TX)/Receive (RX) timing, dropped frames and Microcontroller Unit (MCU) Universal Asynchronous Receiver/Transmitter (UART)/Serial Wire Output (SWO) logs.

Start read-only with a dummy sensor and no motor actuation. Once real Classic drivers and task/event configuration work, add robust memory/diagnostic/error management.

## Scope of verification

At each milestone, record: exact sources/commit, compiler version, board firmware hashes, hardware revisions, wiring, test commands, raw observations and known limitations. A screenshot or "it seems to work" is weaker than deterministic test/trace with counts and timestamps.
