# 09 — Expanded terminology, module behavior and primary references

**Naming rule:** Expand technical names inline where used in these guides, not only here. Code identifiers (`CanIf`, `PduR`, `Dcm`) and wire bytes (`0x7E0`, `0x201`) remain unchanged inside code because they must match real APIs and the test trace. **Microcontroller Abstraction Layer (MCAL)** is the hardware-dependent driver layer; **MACL** is a spelling error. AUTOSAR is *Automotive Open System Architecture*.

## Architecture: full name, purpose, and observable result

| Full name and standard abbreviation | Input → output; what it actually does | Observe or compare |
| --- | --- | --- |
| Electronic Control Unit (ECU) | Complete controller comprising processing hardware, software, interfaces and power; an Electronic Control Unit (ECU) implements a vehicle function. | The host's `lab_ecu` is a one-process teaching substitute; the STM32H723 will eventually be an actual Electronic Control Unit (ECU). |
| Software Component (SWC) | Receives application inputs via ports and runs a function; publishes typed values via other ports. It does not need to name the physical bus. | `lab_application_run_10ms` increments a synthetic speed by `5 cm/s` per activation; this is **software-component-like** code, not an actual generated component. |
| Virtual Functional Bus (VFB) | System-design abstraction connecting software-component ports before the integrator assigns them to processors and networks. | In the design, a speed provider and speed consumer can be connected without giving the provider a CAN identifier. |
| Runtime Environment (RTE) | Generated communication glue that binds software-component ports, local calls and Basic Software services. It is **not** the operating-system task scheduler. | The repo's `lab_rte_com_publish_speed` is only a handwritten illustration; there is no Runtime Environment (RTE) generation step in this repo. |
| Basic Software (BSW) | Platform infrastructure: communication, diagnostics, operating-system scheduling, memory, state control, hardware abstractions, drivers. | Real implementations exist separately in AutoAS/Trampoline; the teaching equivalents are partial C functions. |
| Microcontroller Abstraction Layer (MCAL) | MCU-dependent peripheral drivers under Basic Software: initialize controller registers, transmit/receive via CAN FIFOs, report errors/interrupts. | Real H723 FDCAN / M416 CAN drivers are **not yet** installed; `src/platform/lab_transport.c` substitutes host transport. |
| Operating System (OS) | Activates tasks/interrupts under a configured schedule and manages resources, events and time services; it does not pack CAN signal bits. | Trampoline's `periodic.oil` configures a periodic task and alarm; the compiled POSIX example prints nine activations, then exits. |
| Electronic Control Unit Configuration (ECUC) | Machine-readable configuration of Electronic Control Unit (ECU) software modules and their parameters, used by code-generation tools. | An Electronic Control Unit (ECU) configuration must specify CAN controller, Protocol Data Unit (PDU) routing and scheduling; `config/network.json` is just teaching data, **not** ECUC. |
| AUTOSAR XML (ARXML) | XML (Extensible Markup Language) serialization of versioned AUTOSAR descriptions, including software components, ports, mappings or Electronic Control Unit (ECU) configuration. | Not present here as generated configuration; OIL for Trampoline is a separate OS-only format. |
| OSEK Implementation Language (OIL) | Declarative input specifying OSEK/Trampoline OS tasks, alarms, counters, resources and application modes. | `third_party/trampoline/examples/posix/periodic/periodic.oil` generates POSIX OS configuration and task code. |
| Trampoline configuration generator (GOIL) | Trampoline's actual compiler/tool for OIL-based OS configuration; not a complete Classic Platform Runtime Environment (RTE) generator. | `bash tools/run-trampoline-posix.sh` runs GOIL, builds the example and executes it. |

## Communication and diagnostics: do not confuse neighboring layers

These modules are not interchangeable. A Controller Area Network (CAN) frame can be delivered correctly while the software component or diagnostic service still fails.

| Full name and standard abbreviation | Specific responsibility and boundary | Example |
| --- | --- | --- |
| AUTOSAR Communication module (Com) | Converts software data signals into or out of interaction-layer Protocol Data Units (I-PDUs) with configured byte order/bit position, and handles configured transmission behavior. | Converts speed `50 cm/s` into `32 00` at byte offsets 0–1. Local source: `src/core/lab_com.c` (simplified). |
| Protocol Data Unit Router (PduR) | Routes a configured Protocol Data Unit (PDU) between upper modules and lower bus interfaces or transport modules; it does not decide speed-unit scaling or bit timing. | Routes speed Protocol Data Unit (PDU) toward CAN Interface, or diagnostic Protocol Data Unit (PDU) between CAN Transport Layer and Diagnostic Communication Manager. `lab_pdur_canif_transmit` combines boundaries for teaching. |
| CAN Interface (CanIf) | Maps upper-layer CAN transmit-PDU handles to a CAN controller/identifier; handles driver indication and transmit confirmation abstractions. | Identifier `0x201` is associated with the speed Protocol Data Unit (PDU). A real CAN Interface (CanIf) has configured controller/Protocol Data Unit (PDU) tables, missing from this repo. |
| CAN driver (Can, a Microcontroller Abstraction Layer (MCAL) module) | Programs hardware controller FIFOs, filters, bit timing and transmit/receive operations. | This is the real FDCAN peripheral driver missing from Linux virtual mode, not the same as the CAN Interface (CanIf). |
| CAN Transport Layer (CanTp) | Implements ISO 15765-2 transport segmentation/reassembly, First/Consecutive/Flow Control framing and timeouts. | Response to `22 F1 90` needs one First Frame and two Consecutive Frames; the tester supplies Flow Control. Local teaching subset: `lab_cantp.c`. |
| Diagnostic Communication Manager (Dcm) | Parses a Unified Diagnostic Services (UDS) request (service/subfunction/identifier), validates preconditions and returns a service response or Negative Response Code. | Request `10 03` produces positive response `50 03 ...`; invalid identifier `22 F1 99` gives `7F 22 31`. |
| Diagnostic Event Manager (Dem) | Records diagnostic events, event status and Diagnostic Trouble Codes (DTCs); offers this information to diagnostic services. | Not implemented in the sample; answering `0x22` alone does not demonstrate Dem support. |
| Development Error Tracer (Det) | Receives development-time internal API/configuration misuse reports from modules; **not** an on-vehicle DTC database. | No Det integration in the sample; an invalid local function argument currently returns an error. |
| Communication Manager (ComM) | Arbitrates requests for communication modes (for example, full communication vs no communication). | Distinct from Com signal packing; not implemented by this project. |
| CAN State Manager (CanSM) | Manages CAN controller/network state transitions, including configured recovery from bus-off. | A passing UDP frame test does not exercise hardware CAN bus-off/recovery. |
| CAN Network Management (CanNm) | Exchanges network-management messages so Electronic Control Units (ECUs) coordinate wake/sleep and participation. | No wake/sleep protocol is implemented by the host example. |
| Electronic Control Unit State Manager (EcuM) | Coordinates startup, shutdown and wakeup configuration/state transitions. | A simple `lab_init` call is not an EcuM implementation. |
| Basic Software Mode Manager (BswM) | Evaluates configured mode requests and controls Basic Software (BSW) module modes/actions. | Switching a local integer diagnostic-session field does not implement BswM. |
| Basic Software Scheduler (SchM) | Supplies Basic Software (BSW) scheduled-function integration and exclusive-area coordination where configured. | Separate from Software Component (SWC) runnables and OS priority scheduling; absent here. |
| Nonvolatile Memory Manager (NvM) | Validates and manages configured persistent-data blocks and interacts with memory interfaces. | A future calibration/sensor-offset block belongs here; the test resets state in RAM. |
| Memory Abstraction Interface (MemIf), Flash EEPROM Emulation (Fee), EEPROM Abstraction (Ea) | Memory services map generic persistent-block operations toward flash/EEPROM implementations. | Flash-storage backend and wear handling are not part of the present simulation. |
| Input/Output Hardware Abstraction (IoHwAb), Digital Input/Output (Dio), General-Purpose Timer (Gpt) | IoHwAb gives application-facing I/O abstractions; Dio/Gpt are microcontroller-driver services. | On a board, Dio might read a switch and Gpt provide a counter. Neither is simulated as AUTOSAR Basic Software (BSW) here. |
| Watchdog Driver (Wdg), Watchdog Manager (WdgM), Function Inhibition Manager (FiM) | Watchdog driver services hardware; manager supervises timing/alive conditions; inhibition manager disables functions based on diagnostic status. | Present lab does not prove any watchdog or failure-inhibition behavior. |

## Frame, transport and tool terms

| Full name and abbreviation | Meaning in a trace or experiment |
| --- | --- |
| Controller Area Network (CAN) | Arbitration-based bus protocol used for messages with 11-bit or 29-bit identifiers. Identifier `0x201` chooses priority/message meaning but provides no authentication. |
| Controller Area Network Flexible Data-Rate (CAN FD) | CAN variant supporting 0–64 payload bytes; optional Bit Rate Switch can use a faster data phase. The virtual demo's 16-byte payload does **not** prove physical CAN FD timing. |
| Flexible Data-rate CAN controller (FDCAN) | MCU CAN FD peripheral, such as the STM32H723's FDCAN; needs a physical CAN transceiver to connect to CAN_H/CAN_L. |
| Bit Rate Switch (BRS) | Frame flag allowing faster data-phase signaling, when both controllers/transceivers and timing configurations support it. |
| Data Length Code (DLC) | Encoded CAN/FD data-size field. For CAN FD, DLC values 9–15 correspond to 12, 16, 20, 24, 32, 48 and 64 bytes, not 9–15 bytes. |
| Acknowledgment (ACK) | A successful transmitting node normally requires at least one other node to acknowledge the frame; virtual UDP relaying does not model this. |
| Cyclic Redundancy Check (CRC) | Receiver-side error detection field on physical CAN frames; not generated or checked by the UDP relay. |
| ISO 15765-2 Transport Protocol (ISO-TP) | Segments a diagnostic data stream over CAN, using Single Frame, First Frame, Flow Control and Consecutive Frame packet types. |
| Single Frame (SF) | ISO 15765-2 Transport Protocol (ISO-TP) frame holding a short diagnostic payload; `02 10 03` contains the two Unified Diagnostic Services (UDS) bytes `10 03`. |
| First Frame (FF) | Starts a segmented ISO-TP response; `10 14` announces a 20-byte total diagnostic payload. |
| Flow Control frame (FC) | Receiver's permission/timing command; `30 01 00` grants transmission with block size one and minimum separation time zero. |
| Consecutive Frame (CF) | Carries later pieces of a segmented payload with a wrapping 4-bit sequence counter: `21` then `22` in this demo. |
| Protocol Control Information (PCI) | ISO-TP transport header byte(s) selecting Single Frame (SF)/First Frame (FF)/Consecutive Frame (CF)/Flow Control frame (FC) and related length/sequence/flow control. |
| Block Size (BS) | Flow Control parameter: maximum number of Consecutive Frames before another Flow Control frame; zero means no further block-size pauses. |
| Minimum Separation Time (STmin) | Flow Control parameter specifying minimum gap between sent Consecutive Frames; units depend on encoded value. |
| Unified Diagnostic Services (UDS) | ISO 14229 diagnostic service protocol running over a transport such as ISO-TP over CAN or Diagnostics over IP (DoIP). |
| Service Identifier (SID) | First byte of a Unified Diagnostic Services (UDS) request (`0x10` for DiagnosticSessionControl, `0x22` for ReadDataByIdentifier). Positive response is generally request Service Identifier (SID) + `0x40`. |
| Data Identifier (DID) | Two-byte identifier selected by a Unified Diagnostic Services (UDS) service, e.g., `F1 90` for vehicle identification data. |
| Diagnostic Trouble Code (DTC) | Diagnostic fault classification/status; handled by real Diagnostic Event Manager modules, not generated by the sample. |
| Negative Response Code (NRC) | Third byte of `7F SID NRC` diagnostic negative response: `7F 22 31` means out-of-range DID. |
| Controller Area Network database (DBC) | File format commonly used to specify CAN message identifiers and signal bit layouts; it is **not** a complete AUTOSAR XML/Electronic Control Unit (ECU) configuration. |
| Diagnostics over Internet Protocol (DoIP) | Diagnostic transport over Internet Protocol networks specified by ISO 13400; not part of the current CAN lab. |
| Local Interconnect Network (LIN) | Lower-speed automotive serial bus, distinct from CAN; no LIN driver in this project. |

## Development and verification terminology

- **Microcontroller Unit (MCU):** The on-chip processor, memory and peripherals. The STM32H723 is a Cortex-M7-based MCU; the AT32M416 uses Cortex-M4F.
- **Static Random-Access Memory (SRAM):** On-chip volatile memory holding stacks, queues and runtime data. Measure usage before integrating a large Basic Software (BSW) implementation on the M416's limited SRAM.
- **Hardware Abstraction Layer (HAL):** Vendor-specific library such as STM32CubeH7 HAL. HAL functions can drive FDCAN but are **not automatically compatible with AUTOSAR Microcontroller Abstraction Layer (MCAL) requirements**.
- **Real-Time Operating System (RTOS):** OS with facilities for bounded or predictable task timing; verify worst-case delays on hardware rather than assuming a host simulation is real time.
- **Worst-Case Execution Time (WCET):** Maximum execution duration for a task under defined conditions; `C=1 ms` at `T=10 ms` contributes 10% nominal utilization.
- **Hardware-in-the-Loop (HIL):** Integration verification using real embedded controllers connected to a controlled simulator/test harness. Rootless UDP is software-only, **not HIL**.
- **Serial Wire Debug (SWD):** ARM debug/programming signal interface used to load firmware, halt the core, inspect memory and recover from bad firmware.
- **Universal Asynchronous Receiver/Transmitter (UART):** Basic serial controller often used to report boot status, CAN error counters and firmware version.
- **Portable Operating System Interface (POSIX):** Unix-compatible API specification, used by Trampoline's Linux host simulation; does not provide MCU timing guarantees.
- **Application Programming Interface (API):** Callable contract between modules. Naming a C function `Rte_Read` does not make it a generated AUTOSAR Runtime Environment (RTE) Application Programming Interface (API).
- **Software Development Kit (SDK):** Vendor-supplied headers, drivers, startup code, linker definitions and examples needed for a real MCU project.
- **General-Purpose Input/Output (GPIO):** Digital MCU pin control; a CAN controller's TX/RX logic pins cannot replace a differential CAN transceiver.
- **Phase-Locked Loop (PLL):** Clock circuit multiplying/dividing an oscillator reference; the M416 CAN FD timing depends on correct vendor-specific PLL configuration.

## Primary references

1. [AUTOSAR Classic Platform](https://www.autosar.org/standards/classic-platform/) — official architecture, specifications and release selection; read layers and configuration methodology.
2. [AUTOSAR Adaptive Platform](https://www.autosar.org/standards/adaptive-platform/) — different process/service model; not the same as Classic's statically configured Electronic Control Unit (ECU) stack.
3. [Linux kernel SocketCAN documentation](https://docs.kernel.org/networking/can.html) — canonical `struct can_frame`, `struct canfd_frame`, and `CAN_RAW_FD_FRAMES` behavior.
4. [ST AN5348 FDCAN application note](https://www.st.com/resource/en/application_note/dm00625700-fdcan-peripheral-on-stm32-devices-stmicroelectronics.pdf) — hardware controller initialization and bit timing; confirm correct device family.
5. [Artery AT32M416 product and documents](https://www.arterychip.com/en/product/AT32M416.jsp) — specific device manual, clock and CAN FD driver requirements.
6. [Trampoline RTOS source](https://github.com/TrampolineRTOS/trampoline) — OIL configuration generator and real POSIX OS implementation.
7. [AutoAS source](https://github.com/autoas/as) — inspect Communication module (Com), Protocol Data Unit Router (PduR), CAN Interface (CanIf), CAN Transport Layer (CanTp), and Diagnostic Communication Manager (Dcm), respecting GPLv3/commercial licensing terms.

For exact wire-level requirements use the relevant ISO 11898 (CAN), ISO 15765-2 (transport) and ISO 14229 (Unified Diagnostic Services (UDS)) specifications; educational simplifications do not replace conformance testing.

**Scope warning:** The local `src/core/` is AUTOSAR-*inspired*, not AUTOSAR-conformant; its simple dispatcher, partial CAN transport and handwritten API calls are explicitly separated from the real upstream Trampoline and AutoAS sources.
