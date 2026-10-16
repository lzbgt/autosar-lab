# 01 — AUTOSAR architecture from first principles

## Why AUTOSAR exists

Without a common Electronic Control Unit (ECU) architecture, each vendor tends to bind its application to a specific microcontroller's registers, scheduler, diagnostic functions and network drivers. AUTOSAR separates these responsibilities so a vehicle function can be configured for another Electronic Control Unit (ECU) without rewriting all of its communication and driver code. Migrating the same control function from one Electronic Control Unit (ECU) to another causes expensive rewrites. AUTOSAR specifies **interfaces, module roles, data/configuration formats and methodology**, reducing the integration burden (but not eliminating configuration and vendor effort).

## Classic Platform versus Adaptive Platform

| | Classic Platform (CP) | Adaptive Platform (AP) |
| --- | --- | --- |
| Target | Predictable microcontroller Electronic Control Unit (ECU) | Higher-performance Portable Operating System Interface (POSIX)-class Electronic Control Unit (ECU) |
| Execution | Static scheduling/Operating System (OS) configuration, runnables | Processes/execution management, services |
| Typical communication | Signal/Protocol Data Unit (PDU) with Controller Area Network (CAN), LIN, FlexRay, Ethernet | Service-oriented Ethernet/ara::com, SOME/IP common |
| Main application abstraction | Software Component (SWC) and Runtime Environment (RTE) | Adaptive application and ARA interfaces |
| This project's focus | **Yes** | Conceptual comparison only |

The main architecture in Classic is:

~~~text
System / vehicle functions                         DESIGN TIME
   SensorSWC ── sender port ─┐                ARXML descriptions
                            │                       │
   ControllerSWC ◀── receiver port                   ▼
        │                application             System + ECU config
   Rte_Read/Write(...)      │                       │
   ───────────────────────────────────          generators
                  RTE                                │
   ───────────────────────────────────          generated RTE, BSW cfg
       services       ECU abstraction                 │
   [OS EcuM BswM]     [CanIf / Dio]                    ▼
   [Com PduR Dcm]     [NvM ...]                  link firmware
   [CanTp Dem...]           │                          │
   ───────────────────────────────────                 ▼
        MCAL: Can, Dio, Gpt, Spi                 flash to MCU
   ───────────────────────────────────
          ECU microcontroller/peripherals
~~~

This diagram is conceptual: module placement in formal diagrams varies by functional stack and AUTOSAR release.

### The three Classic software layers

1. **Application:** One or more Software Components (SWCs) containing algorithms as *runnables*. Software Components (SWCs) declare provided/required ports and interfaces; the application should not depend on the exact Controller Area Network (CAN) controller register map.
2. **Runtime Environment (RTE):** Generated communication/call glue that maps Software Component (SWC) ports to other local Software Components (SWCs) or to Electronic Control Unit (ECU) services / AUTOSAR Communication module (Com) signals across the network. **Runtime Environment (RTE) is not a physical Controller Area Network (CAN) driver and not a Real-Time Operating System (RTOS) scheduler.**
3. **Basic Software (BSW):** Reusable infrastructure below the generated Runtime Environment (RTE): operating-system scheduling, communication/diagnostic modules, nonvolatile-memory services, and hardware abstraction. The **Microcontroller Abstraction Layer (MCAL)** is the hardware-dependent lower part of Basic Software (BSW): its CAN controller driver configures the peripheral, places a frame in a hardware transmit queue, and reports completion/errors. The CAN Interface (CanIf) module above it is a different layer. **Microcontroller Abstraction Layer (MCAL)**, not *MACL*, is the correct abbreviation.

## Work through one real example, one module at a time

The checked-in application produces a synthetic speed value. Ten activations of `lab_application_run_10ms` (one every 10 ms) increment speed by 5 cm/s each. At 100 ms, the speed is **50 cm/s = hexadecimal 0x0032**. With little-endian serialization, the first two payload bytes are `32 00`; CAN identifier `0x201` is separate from the eight payload bytes.

| Full name, role | Value entering this boundary | What comes out | Where to inspect |
| --- | --- | --- | --- |
| Application **Software Component (SWC)** | Timer activation, previous speed state | A typed speed value `50 cm/s` | `src/core/lab_ecu.c`, `lab_application_run_10ms` |
| Generated **Runtime Environment (RTE)** | A software-component port value | A call or signal transfer to the configured destination, independent of physical bus | **No Runtime Environment (RTE) generator in this lab.** A handwritten call simulates the idea. |
| **AUTOSAR Communication module (Com)** | Typed integer `0x0032` and message configuration | Encoded byte sequence `32 00 00 01 00 00 00 00`; no bus arbitration yet | `src/core/lab_com.c` packs the bytes directly, using a simplified substitute |
| **Protocol Data Unit Router (PduR)** | An encoded Protocol Data Unit (PDU) and configured routing identifiers | The same Protocol Data Unit (PDU) handed to its selected lower module; it does **not** itself serialize speed or set CAN bit timing | `lab_pdur_canif_transmit` merges routing and CAN-interface responsibilities for teaching |
| **Controller Area Network Interface (CanIf)** | A Protocol Data Unit (PDU) handle and payload | Configured CAN transmit request (e.g., CAN identifier `0x201`) toward the controller driver | Still merged into `lab_pdur_canif_transmit`; no configured CAN Interface (CanIf) stack |
| **Microcontroller Abstraction Layer (MCAL) CAN driver** | CAN identifier, payload, frame length, controller state | A hardware transmit queue entry; then transmit confirmation or error state | **Absent on Linux.** `src/platform/lab_transport.c` sends a virtual frame instead. |
| Physical **Controller Area Network (CAN) bus** | Encoded frame from a transceiver | Differential CAN_H/CAN_L signaling with arbitration, acknowledgement and error handling | **Not emulated by UDP or Linux vcan.** Requires physical controllers/transceivers. |

The receiver follows the inverse signal path: CAN controller → CAN Interface (CanIf) → Protocol Data Unit Router (PduR) → Communication module (Com) → generated Runtime Environment (RTE) → receiver Software Component (SWC). Real deployments require signal configuration, buffer ownership, notifications and task scheduling at each boundary; the sample only implements a narrow, single-message teaching path.

**Why this distinction matters:** A working `lab_pdur_canif_transmit` does not establish that a generated Runtime Environment (RTE), a real AUTOSAR Communication module (Com), or an AUTOSAR Microcontroller Abstraction Layer (MCAL) has been installed. It proves only that the tutorial sent the expected bytes through its own simplified functions.

## Diagnostic requests use a different path

A tester requesting diagnostic session `0x03` sends Classical CAN identifier `0x7E0` with payload `02 10 03 00 00 00 00 00`. The `02` is the ISO 15765-2 Transport Protocol (ISO-TP) Single Frame payload length; the `10` is the Unified Diagnostic Services (UDS) **Service Identifier (SID)** for DiagnosticSessionControl; `03` selects extended session. The expected reply uses identifier `0x7E8` and starts `06 50 03 00 32 01 F4`.

Actual AUTOSAR receive chain:

~~~text
CAN hardware -> CAN driver (MCAL) -> CanIf -> CanTp -> PduR -> Dcm
                                                    UDS service
CAN hardware <- CAN driver (MCAL) <- CanIf <- CanTp <- PduR <- Dcm
~~~

The AUTOSAR **CAN Transport Layer (CanTp)** removes Single Frame or reassembles multi-frame transport headers; the **Protocol Data Unit Router (PduR)** forwards the diagnostic Protocol Data Unit (PDU) to the **Diagnostic Communication Manager (Dcm)**; the Diagnostic Communication Manager validates the service/session and constructs the reply. In the local code, `lab_cantp_rx_indication` directly calls `lab_dcm_request`, bypassing an actual Protocol Data Unit (PDU) Router. Trace the request with the following executable test:

~~~sh
cd ~/work/autosar-lab
bash tools/run-all.sh
# Inspect exact dispatch and response formation:
rg -n 'lab_cantp_rx_indication|lab_dcm_request|lab_cantp_transmit' src/core
~~~

The raw diagram uses standardized module identifiers (`CanIf`, `CanTp`, `PduR`, `Dcm`, `MCAL`); their full names and distinct input/output responsibilities are explained immediately above and below it.


### Distinguish Basic Software (BSW) responsibilities

| Layer / cluster | Examples | Responsibility |
| --- | --- | --- |
| Service / System | Os, Electronic Control Unit State Manager (EcuM), Basic Software Mode Manager (BswM), WdgM, Basic Software Scheduler (SchM) | Scheduling, lifecycle, watchdog and module coordination |
| Service / Communication | Communication module (Com), Protocol Data Unit Router (PduR), Diagnostic Communication Manager (Dcm), CAN Transport Layer (CanTp), Communication Manager (ComM), Nm, CAN State Manager (CanSM) | Signals, routing, diagnostics, network state |
| Service / Memory | Nonvolatile Memory Manager (NvM), Memory Abstraction Interface (MemIf), Flash EEPROM Emulation (Fee), EEPROM Abstraction (Ea) | Persistent blocks, abstraction over flash/EEPROM |
| Service / Diagnostics | Diagnostic Event Manager (Dem), Development Error Tracer (Det), Function Inhibition Manager (FiM) | Event/Diagnostic Trouble Code (DTC) handling, development errors and function inhibition |
| Electronic Control Unit (ECU) abstraction | CAN Interface (CanIf), Input/Output Hardware Abstraction (IoHwAb), Adc interface abstractions | Device/Electronic Control Unit (ECU)-level interface independent of specific Microcontroller Unit (MCU) |
| Microcontroller Abstraction Layer (MCAL) | Can, Digital Input/Output driver (Dio), General-Purpose Timer driver (Gpt), Mcu, Port, Spi, Watchdog driver (Wdg) | Microcontroller Unit (MCU)-specific drivers and peripheral configuration |
| Complex device drivers | CDD | Specialized interfaces when standard abstractions are insufficient |

**Practical rule:** Porting from STM32H723 to AT32M416 should primarily change drivers, board settings and Basic Software (BSW) configuration. A portable Software Component (SWC)/control algorithm should change minimally, provided functional and timing constraints are met.

## The virtual functional bus (VFB)

At system design time a Software Component (SWC) refers to **ports**, not physical wires. The Virtual Functional Bus (VFB) is a logical model of their communication. When software components are mapped to Electronic Control Units (ECUs), the Runtime Environment (RTE) generator and communication stack provide actual data paths (intra-Electronic Control Unit (ECU) call or inter-Electronic Control Unit (ECU) Protocol Data Unit (PDU)).

Example: SensorSWC provides `VehicleSpeed_cm_s` through a sender port. ControllerSWC consumes it. If on different Electronic Control Units (ECUs), generated configuration maps the typed signal to a Controller Area Network (CAN) Protocol Data Unit (PDU); application code does not hardcode Controller Area Network (CAN) ID 0x201.

Our teaching core intentionally does **not** have a real Runtime Environment (RTE) generator: the Runtime Environment (RTE)-like connection is a handwritten C call in `lab_ecu.c`/`lab_com.c`. This makes flows inspectable but does not demonstrate generated system configuration.

## Design-time workflow you must understand

1. Define requirements: cycle times, data types/ranges, diagnostics, safety.
2. Define Software Components (SWCs), ports, interfaces, runnables and events, using AUTOSAR descriptions.
3. Design network: Electronic Control Unit (ECU) mapping, Protocol Data Unit (PDU) signals, Controller Area Network (CAN) IDs, timing and diagnostics.
4. Configure Electronic Control Unit (ECU) Basic Software (BSW) and Microcontroller Abstraction Layer (MCAL) (controllers, filters, routing tables, buffers, tasks).
5. Generate Runtime Environment (RTE), Basic Software (BSW) configuration, Operating System (OS) configuration and vendor drivers as applicable.
6. Compile handwritten/generated code, link against Operating System (OS)/Basic Software (BSW), verify maps and resources.
7. Unit, component, integration and hardware-in-the-loop test. Flash and monitor.
8. Validate timing, memory, diagnostics, network and failure modes.

Descriptions commonly use **AUTOSAR XML (ARXML)**, an XML representation conforming to AUTOSAR schemas. A DBC is a signal-network database and does **not** replace complete AUTOSAR Software Component (SWC)/Runtime Environment (RTE)/Basic Software (BSW) configuration. The local `config/network.json` is a teaching reference only, not a generated AUTOSAR XML (ARXML).

## Exercises

- On paper route a 2-byte velocity from Microcontroller Unit (MCU) A to Microcontroller Unit (MCU) B. Label sender Software Component (SWC) → Runtime Environment (RTE) → AUTOSAR Communication module (Com) → Protocol Data Unit Router (PduR) → CAN Interface (CanIf) → Can, then Can → CAN Interface (CanIf) → Protocol Data Unit Router (PduR) → AUTOSAR Communication module (Com) → Runtime Environment (RTE) → receiver Software Component (SWC).
- Route a diagnostic ReadDataByIdentifier from external tester to Diagnostic Communication Manager (Dcm): Can → CAN Interface (CanIf) → CAN Transport Layer (CanTp) → Protocol Data Unit Router (PduR) → Diagnostic Communication Manager (Dcm) (and reverse for response).
- Which modules move between different MCUs? Which are intended to be reusable?
- Explain why access to physical Serial Wire Debug (SWD)/JTAG is **not** the same as application-level Unified Diagnostic Services (UDS) diagnostics.

Source of truth: https://www.autosar.org/standards/classic-platform/
