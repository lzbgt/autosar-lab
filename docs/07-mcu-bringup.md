# 07 — Move the lab to your STM32H723 and AT32M416

**Current status:** Portable C core compiled on the Arch host for Cortex-M7 and Cortex-M4. This is **not** board flash or real Controller Area Network (CAN) verification. Microcontroller Unit (MCU) drivers and board wiring remain to be integrated, and no claim of a working AUTOSAR Electronic Control Unit (ECU) on physical hardware is made.

## Microcontroller Unit (MCU) roles

| Role | Part | Core / resources | Rationale |
| --- | --- | --- | --- |
| Main Electronic Control Unit (ECU) | STM32H723 | Cortex-M7 with FDCAN | More headroom for diagnostics and future Real-Time Operating System (RTOS)/Basic Software (BSW) |
| Peer Electronic Control Unit (ECU) | AT32M416CBT7 | Cortex-M4F, 128KB Flash / 16KB Static Random-Access Memory (SRAM), one Controller Area Network Flexible Data-Rate (CAN FD) | Low-cost constrained sensor/motor-controller peer |
| Optional legacy Controller Area Network (CAN) | STM32F407 | bxCAN Classical Controller Area Network (CAN) | Useful for Classic Operating System (OS)/driver learning; **not** for FD traffic |

AT32M416 hardware capabilities confirmed by the [official product page](https://www.arterychip.com/en/product/AT32M416.jsp). Exact H723 pin assignments and clock tree depend on the **specific PCB**, not only the Microcontroller Unit (MCU) type.

## Wiring and minimum external hardware

~~~text
      STM32H723                           AT32M416
   FDCAN1_TX/RX                          CANFD_TX/RX
         │                                    │
    FD transceiver A                     FD transceiver B
      CANH ───────────── CAN_H ─────────────── CANH
      CANL ───────────── CAN_L ─────────────── CANL
       GND ───────────── GND ───────────────── GND
         └─ 120Ω at left end       120Ω at right end ┘
~~~

Use two **Controller Area Network Flexible Data-Rate (CAN FD)-compatible** transceivers (for example TCAN3413 at 3.3V, or MCP2562FD with correct VDD/VIO). A Controller Area Network (CAN) controller requires a transceiver; using General-Purpose Input/Output (GPIO) directly on CAN_H/L is incorrect. Keep a short twisted CAN_H/CAN_L pair. Supply common ground and correct voltage levels. Terminate only physical ends of the bus, typically one 120Ω at each end. Power-off between-line resistance should be close to 60Ω (barring onboard bias/termination effects). Check that your modules do not add extra 120Ω terminators.

**Do not assume a pin:** inspect the H723 board schematic/CubeMX AF assignment and AT32M416 reference manual, then document Microcontroller Unit (MCU) Transmit (TX)/Receive (RX) port/pin and transceiver standby pins in hardware-specific notes. Verify oscillator and Phase-Locked Loop (PLL) constraints before enabling Controller Area Network Flexible Data-Rate (CAN FD) bit timing. The AT32M416 datasheet requires Controller Area Network (CAN) clocking sourced from HEXT-based Phase-Locked Loop (PLL) for transmission quality.

## First real test: Classical Controller Area Network (CAN) on both FD-capable MCUs

1. Prove Microcontroller Unit (MCU) clocks, Serial Wire Debug (SWD) and serial log using vendor blinking sample; **backup** anything that must be retained.
2. Add driver read-only counters (Receive (RX) accepted, Receive (RX) overflow, Transmit (TX) acknowledged, error passive/bus-off). Expose via Universal Asynchronous Receiver/Transmitter (UART)/Serial Wire Output (SWO).
3. Configure each controller's nominal bit timing for **500 kbit/s**, 11-bit standard IDs and internal filters for 0x201 / 0x7E0 / 0x7E8.
4. Run one Microcontroller Unit (MCU) as 0x201 publisher and the other as receiver. Verify Receive (RX) count, interarrival distribution and data fields using an independent Controller Area Network (CAN) analyzer if available.
5. Test link-off and bus-disconnected behavior; Transmit (TX) without another normal active node can lack Acknowledgment (ACK) (some modes silently simulate loopback).
6. Only after clean normal-mode wire communication should you attach the tutorial Electronic Control Unit (ECU) core.

## Then Controller Area Network Flexible Data-Rate (CAN FD)

1. Verify both controllers, driver stacks and transceivers explicitly support **ISO Controller Area Network Flexible Data-Rate (CAN FD)** mode.
2. Configure nominal arbitration bitrate at 500 kbit/s. Start data phase at 2 Mbit/s, with Bit Rate Switch (BRS) enabled at both endpoints.
3. Send a test 16-byte frame (the tutorial's 0x201 telemetry uses this size). Confirm Data Length Code (DLC) mapping, message Random-Access Memory (RAM) buffers, filters, Transmit (TX) confirmation and error counters.
4. Inject malformed configuration (only on an isolated bench bus): incompatible sample points/clock, FD disabled receiver, missing termination. Observe error counters, **restore config**, and never leave an actual actuator enabled.
5. Expand to 64-byte Controller Area Network Flexible Data-Rate (CAN FD) payload after the link is stable; update buffer sizing in driver and analysis tools.

Host `vcan` and UDP relay **cannot** verify nominal/data bitrate. Do not confuse "FD flag received in a virtual frame" with FD transceiver compatibility.

## Microcontroller Unit (MCU) code port contract

One function adapts output:
- `int tx(void *context, const LabFrame *frame)` should validate identifier/payload, map Classical/FD ID + length + Bit Rate Switch (BRS), queue the frame through the Microcontroller Unit (MCU) driver and return 0 on accepted Transmit (TX) or -1 on failure.
- For each driver Receive (RX) notification, convert controller packet into `LabFrame` and call `lab_on_frame(&ecu, &packet)` **from an application task**, not heavy code directly from ISR.
- A 1ms or 10ms periodic timer calls `lab_tick(&ecu, elapsed_monotonic_ms)` at least fast enough to maintain the intended 10ms runnable and transport pacing.
- Use bounded queues/ring buffers and check transmission confirmation/error state; `LabTransmit` returning 0 means accepted for transmission, **not** necessarily ACKed on wire.

A real AUTOSAR Basic Software (BSW) integration should replace the teaching Runtime Environment (RTE)/communication/diagnostic stack with generated configuration and compliant modules, instead of directly mixing with vendor Hardware Abstraction Layer (HAL) callbacks.

## Firmware toolchains / debugger

**H723:** STM32CubeH7 Hardware Abstraction Layer (HAL) + CubeMX project, arm-none-eabi-gcc, J-Link or OpenOCD as supported by your board. Consult ST's AN5348 for FDCAN initialization, shared message Random-Access Memory (RAM), FDCAN filters, bit timing and Data Length Code (DLC) mapping:
https://www.st.com/content/st_com/en/technical-documents/an5348-introduction-to-fdcan-peripherals-for-stm32-mcus-stmicroelectronics.html

**M416:** official Artery firmware library/BSP and reference manual:
https://www.arterychip.com/en/product/AT32M416.jsp
J-Link/AT-Link support depends on probe firmware and board; verify actual Serial Wire Debug (SWD) wiring and reset pins against board schematics.

For either target:
- Verify Serial Wire Debug (SWD) probe attaches to the exact CPU and halted PC points to valid startup code.
- After vendor project links, use `arm-none-eabi-size firmware.elf` and inspect `.map`.
- Add read-only diagnostics: Universal Asynchronous Receiver/Transmitter (UART) firmware version/hash, task count, Controller Area Network (CAN) Receive (RX)/Transmit (TX) counts, lost frames, bus-off state.
- Preserve a known-good firmware and reliable Serial Wire Debug (SWD) recovery method.

## Test plan and acceptance gates

| Stage | Gate |
| --- | --- |
| CPU/Serial Wire Debug (SWD) | Deterministic LED/Universal Asynchronous Receiver/Transmitter (UART) after cold boot and reset |
| Controller Area Network (CAN) Classical | Independent Receive (RX), Transmit (TX) + Acknowledgment (ACK) at 500 kbit/s without bus errors |
| Controller Area Network Flexible Data-Rate (CAN FD) | 16-byte 0x201 Bit Rate Switch (BRS) frame, 2 Mbit/s data phase, compatible ISO FD |
| Portable core | Velocity payload matches 0x201 definition on wire |
| Diagnostic | 0x10, 0x22 F187, 0x22 F190 via external tester, Flow Control frame (FC)/Minimum Separation Time (STmin) works |
| Stability | Bounded queue, no unbounded heap, faults recovered, watchdog tested |
| Operating System (OS)/Basic Software (BSW) | Only after validated third-party or vendor AUTOSAR integration on target |

See `hardware/stm32h723/README.md` and `hardware/at32m416/README.md` for board-specific checklists.
