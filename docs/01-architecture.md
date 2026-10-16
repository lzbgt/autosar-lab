# 01 — AUTOSAR architecture from first principles

## Why AUTOSAR exists

Without a common ECU architecture, each vendor builds applications tightly coupled to MCU registers, proprietary networking, OS calls and diagnostic stacks. Migrating the same control function from one ECU to another causes expensive rewrites. AUTOSAR specifies **interfaces, module roles, data/configuration formats and methodology**, reducing the integration burden (but not eliminating configuration and vendor effort).

## Classic Platform versus Adaptive Platform

| | Classic Platform (CP) | Adaptive Platform (AP) |
| --- | --- | --- |
| Target | Predictable microcontroller ECU | Higher-performance POSIX-class ECU |
| Execution | Static scheduling/OS configuration, runnables | Processes/execution management, services |
| Typical communication | Signal/PDU with CAN, LIN, FlexRay, Ethernet | Service-oriented Ethernet/ara::com, SOME/IP common |
| Main application abstraction | Software Component (SWC) and RTE | Adaptive application and ARA interfaces |
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

1. **Application:** One or more SWCs containing algorithms as *runnables*. SWCs declare provided/required ports and interfaces; the application should not depend on the exact CAN controller register map.
2. **RTE (Runtime Environment):** Generated communication/call glue that maps SWC ports to other local SWCs or to ECU services / COM signals across the network. **RTE is not a physical CAN driver and not an RTOS scheduler.**
3. **BSW (Basic Software):** OS and infrastructure services, communication/memory and hardware abstraction, MCAL. Actual module sets depend on requirements and platform.

### Distinguish BSW responsibilities

| Layer / cluster | Examples | Responsibility |
| --- | --- | --- |
| Service / System | Os, EcuM, BswM, WdgM, SchM | Scheduling, lifecycle, watchdog and module coordination |
| Service / Communication | Com, PduR, Dcm, CanTp, ComM, Nm, CanSM | Signals, routing, diagnostics, network state |
| Service / Memory | NvM, MemIf, Fee, Ea | Persistent blocks, abstraction over flash/EEPROM |
| Service / Diagnostics | Dem, Det, FiM | Event/DTC handling, development errors and function inhibition |
| ECU abstraction | CanIf, IoHwAb, Adc interface abstractions | Device/ECU-level interface independent of specific MCU |
| MCAL | Can, Dio, Gpt, Mcu, Port, Spi, Wdg | MCU-specific drivers and peripheral configuration |
| Complex device drivers | CDD | Specialized interfaces when standard abstractions are insufficient |

**Practical rule:** Porting from STM32H723 to AT32M416 should primarily change drivers, board settings and BSW configuration. A portable SWC/control algorithm should change minimally, provided functional and timing constraints are met.

## The virtual functional bus (VFB)

At system design time an SWC refers to **ports**, not physical wires. The VFB is a logical model of their communication. When software components are mapped to ECUs, the RTE generator and communication stack provide actual data paths (intra-ECU call or inter-ECU PDU).

Example: SensorSWC provides `VehicleSpeed_cm_s` through a sender port. ControllerSWC consumes it. If on different ECUs, generated configuration maps the typed signal to a CAN PDU; application code does not hardcode CAN ID 0x201.

Our teaching core intentionally does **not** have a real RTE generator: the RTE-like connection is a handwritten C call in `lab_ecu.c`/`lab_com.c`. This makes flows inspectable but does not demonstrate generated system configuration.

## Design-time workflow you must understand

1. Define requirements: cycle times, data types/ranges, diagnostics, safety.
2. Define SWCs, ports, interfaces, runnables and events, using AUTOSAR descriptions.
3. Design network: ECU mapping, PDU signals, CAN IDs, timing and diagnostics.
4. Configure ECU BSW and MCAL (controllers, filters, routing tables, buffers, tasks).
5. Generate RTE, BSW configuration, OS configuration and vendor drivers as applicable.
6. Compile handwritten/generated code, link against OS/BSW, verify maps and resources.
7. Unit, component, integration and hardware-in-the-loop test. Flash and monitor.
8. Validate timing, memory, diagnostics, network and failure modes.

Descriptions commonly use **ARXML**, an XML representation conforming to AUTOSAR schemas. A DBC is a signal-network database and does **not** replace complete AUTOSAR SWC/RTE/BSW configuration. The local `config/network.json` is a teaching reference only, not a generated ARXML.

## Exercises

- On paper route a 2-byte velocity from MCU A to MCU B. Label sender SWC → RTE → COM → PduR → CanIf → Can, then Can → CanIf → PduR → COM → RTE → receiver SWC.
- Route a diagnostic ReadDataByIdentifier from external tester to Dcm: Can → CanIf → CanTp → PduR → Dcm (and reverse for response).
- Which modules move between different MCUs? Which are intended to be reusable?
- Explain why access to physical SWD/JTAG is **not** the same as application-level UDS diagnostics.

Source of truth: https://www.autosar.org/standards/classic-platform/
