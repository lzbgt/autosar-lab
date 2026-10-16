# 09 — Glossary and primary references

## Terms used throughout the labs

| Term | Meaning / learning question |
| --- | --- |
| AUTOSAR CP | Classic Platform for deeply embedded ECUs; Application + RTE + BSW |
| AUTOSAR AP | Adaptive Platform for POSIX-class applications and services |
| ECU | Electronic control unit; one processor or processing system doing a defined vehicle function |
| SWC | Software Component: typed application block with ports/runnables |
| VFB | Virtual Functional Bus: conceptual inter-SWC communication at system design time |
| P-Port / R-Port | Provided/required software component ports |
| Runnable | Entry point triggered by event or timing condition |
| Timing event | Cause for runnable activation at configured time interval |
| RTE | Generated Runtime Environment glue between application and infrastructure |
| BSW | Basic Software: OS, services, communication, memory/hardware infrastructure |
| MCAL | MCU abstraction layer: peripheral drivers with standardized interfaces/configuration |
| ECUC | ECU configuration descriptions, used to generate/configure BSW |
| ARXML | AUTOSAR XML exchange descriptions; not a C header |
| OIL | OSEK Implementation Language for OS configuration |
| GOIL | Trampoline tool that processes OIL/ARXML OS configuration |
| DBC | CAN network database describing signals/messages; not full ECU/RTE config |
| OS task / alarm | Execution context / timed triggering mechanism |
| ISR | Interrupt service routine, subject to real-time restrictions |
| SchM | BSW scheduling/exclusive-area handling |
| EcuM | ECU state/startup/shutdown management |
| BswM | Basic software mode control |
| Com | Signal and signal-group packing/unpacking from/to I-PDUs |
| PduR | PDU Router: routes PDUs between upper modules and bus interfaces |
| CanIf | CAN interface abstraction; controller/PDU routing above MCAL |
| Can | MCAL CAN controller driver (not "CAN TP") |
| CanTp | ISO-TP CAN transport segmentation/reassembly and flow control |
| Dcm | Diagnostic communication manager, implements configured diagnostic services |
| Dem | Diagnostic event manager; stores/manages event and DTC statuses |
| Det | Development error tracer |
| NvM | Non-volatile memory block services |
| Fee/Ea | Flash/EEPROM emulation abstraction modules |
| ComM | Communication mode management |
| CanSM | CAN state management |
| Nm / CanNm | Network management / CAN network management |
| DoIP | Diagnostics over IP (ISO 13400 family) |
| UDS | Unified Diagnostic Services (ISO 14229); 0x10 session, 0x22 DID etc |
| DID | Data Identifier used in UDS services like ReadDataByIdentifier |
| DTC | Diagnostic trouble code/status associated with Dem/events |
| NRC | Negative Response Code; third byte of UDS 0x7F response |
| PDU / I-PDU | Protocol data unit; application/user data plus defined stack framing |
| N-PDU | Network-layer PDU, often in transport/network communication |
| CAN ID | 11-bit or 29-bit identifier used for arbitration, **not source authentication** |
| DLC | Data length code encoding physical frame payload size |
| CAN FD | Flexible data-rate variant with up to 64-byte payload and optional BRS |
| BRS | Bit Rate Switch for CAN FD data phase |
| CRC | Cyclic redundancy check used to detect transmission errors |
| SF / FF / CF / FC | ISO-TP Single / First / Consecutive / Flow Control frames |
| BS / STmin | ISO-TP flow-control block size / minimum consecutive-frame interval |
| CAN bus-off | Controller state after severe errors; needs defined recovery |
| vcan | Linux kernel virtual CAN interface for SocketCAN tests |
| SocketCAN | Linux PF_CAN sockets and CAN frame APIs |
| SWD | Serial Wire Debug for firmware programming/debugging |
| HIL | Hardware-in-the-loop system integration testing |
| WCET | Worst-case execution time, input to schedulability analysis |
| ISO 26262 | Road-vehicle functional safety standard (not implied by this demo) |
| ISO/SAE 21434 | Road-vehicle cybersecurity engineering standard |

## Suggested primary reading order

1. [AUTOSAR Classic Platform architecture and release documents](https://www.autosar.org/standards/classic-platform/) — official; current page R25-11 as checked 2026-10-16. Read architecture, methodology, layers and module specifications, not merely marketing diagrams.
2. [AUTOSAR Adaptive Platform](https://www.autosar.org/standards/adaptive-platform/) — contrast with classic.
3. [Linux kernel SocketCAN documentation](https://docs.kernel.org/networking/can.html) — canonical ``can_frame`/`canfd_frame` and `CAN_RAW_FD_FRAMES` behavior.
4. [ST AN5348 FDCAN application note](https://www.st.com/content/st_com/en/technical-documents/an5348-introduction-to-fdcan-peripherals-for-stm32-mcus-stmicroelectronics.html) — hardware controller, bit rate and message RAM.
5. [Artery AT32M416 product, datasheet, RM and BSP](https://www.arterychip.com/en/product/AT32M416.jsp) — hardware-specific CAN FD and clock requirements.
6. [Trampoline RTOS repository](https://github.com/TrampolineRTOS/trampoline) — a working OSEK/AUTOSAR-OS-aligned implementation, with POSIX emulator.
7. [AutoAS BSW repository](https://github.com/autoas/as) — module implementation and documented diagnostic simulator; read license.
8. [COVESA vsomeip](https://github.com/COVESA/vsomeip) — optional service-oriented automotive Ethernet exercise.

For definitive wire requirements consult the relevant ISO standards (CAN: ISO 11898, ISO-TP: ISO 15765-2, UDS: ISO 14229), including applicable editions. Public summaries and educational snippets can omit critical timing and addressing details.

## Terminology hygiene

- **"AUTOSAR-inspired teaching code"** for source under `src/core/`.
- **"OSEK/AUTOSAR OS-aligned real implementation"** for Trampoline.
- **"Open-source AUTOSAR BSW modules"** for AutoAS, subject to actual module coverage and licenses.
- **"AUTOSAR-compliant integrated ECU"** only with appropriate stack, configuration and verified conformance evidence. Nothing in the current repo merits that label.

Do not conflate a working Linux diagnostic demo with certified automotive firmware.
