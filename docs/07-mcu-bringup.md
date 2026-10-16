# 07 — Move the lab to your STM32H723 and AT32M416

**Current status:** Portable C core compiled on the Arch host for Cortex-M7 and Cortex-M4. This is **not** board flash or real CAN verification. MCU drivers and board wiring remain to be integrated, and no claim of a working AUTOSAR ECU on physical hardware is made.

## MCU roles

| Role | Part | Core / resources | Rationale |
| --- | --- | --- | --- |
| Main ECU | STM32H723 | Cortex-M7 with FDCAN | More headroom for diagnostics and future RTOS/BSW |
| Peer ECU | AT32M416CBT7 | Cortex-M4F, 128KB Flash / 16KB SRAM, one CAN FD | Low-cost constrained sensor/motor-controller peer |
| Optional legacy CAN | STM32F407 | bxCAN Classical CAN | Useful for Classic OS/driver learning; **not** for FD traffic |

AT32M416 hardware capabilities confirmed by the [official product page](https://www.arterychip.com/en/product/AT32M416.jsp). Exact H723 pin assignments and clock tree depend on the **specific PCB**, not only the MCU type.

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

Use two **CAN FD-compatible** transceivers (for example TCAN3413 at 3.3V, or MCP2562FD with correct VDD/VIO). A CAN controller requires a transceiver; using GPIO directly on CAN_H/L is incorrect. Keep a short twisted CAN_H/CAN_L pair. Supply common ground and correct voltage levels. Terminate only physical ends of the bus, typically one 120Ω at each end. Power-off between-line resistance should be close to 60Ω (barring onboard bias/termination effects). Check that your modules do not add extra 120Ω terminators.

**Do not assume a pin:** inspect the H723 board schematic/CubeMX AF assignment and AT32M416 reference manual, then document MCU TX/RX port/pin and transceiver standby pins in hardware-specific notes. Verify oscillator and PLL constraints before enabling CAN FD bit timing. The AT32M416 datasheet requires CAN clocking sourced from HEXT-based PLL for transmission quality.

## First real test: Classical CAN on both FD-capable MCUs

1. Prove MCU clocks, SWD and serial log using vendor blinking sample; **backup** anything that must be retained.
2. Add driver read-only counters (RX accepted, RX overflow, TX acknowledged, error passive/bus-off). Expose via UART/SWO.
3. Configure each controller's nominal bit timing for **500 kbit/s**, 11-bit standard IDs and internal filters for 0x201 / 0x7E0 / 0x7E8.
4. Run one MCU as 0x201 publisher and the other as receiver. Verify RX count, interarrival distribution and data fields using an independent CAN analyzer if available.
5. Test link-off and bus-disconnected behavior; TX without another normal active node can lack ACK (some modes silently simulate loopback).
6. Only after clean normal-mode wire communication should you attach the tutorial ECU core.

## Then CAN FD

1. Verify both controllers, driver stacks and transceivers explicitly support **ISO CAN FD** mode.
2. Configure nominal arbitration bitrate at 500 kbit/s. Start data phase at 2 Mbit/s, with BRS enabled at both endpoints.
3. Send a test 16-byte frame (the tutorial's 0x201 telemetry uses this size). Confirm DLC mapping, message RAM buffers, filters, TX confirmation and error counters.
4. Inject malformed configuration (only on an isolated bench bus): incompatible sample points/clock, FD disabled receiver, missing termination. Observe error counters, **restore config**, and never leave an actual actuator enabled.
5. Expand to 64-byte CAN FD payload after the link is stable; update buffer sizing in driver and analysis tools.

Host `vcan` and UDP relay **cannot** verify nominal/data bitrate. Do not confuse "FD flag received in a virtual frame" with FD transceiver compatibility.

## MCU code port contract

One function adapts output:
- `int tx(void *context, const LabFrame *frame)` should validate identifier/payload, map Classical/FD ID + length + BRS, queue the frame through the MCU driver and return 0 on accepted TX or -1 on failure.
- For each driver RX notification, convert controller packet into `LabFrame` and call `lab_on_frame(&ecu, &packet)` **from an application task**, not heavy code directly from ISR.
- A 1ms or 10ms periodic timer calls `lab_tick(&ecu, elapsed_monotonic_ms)` at least fast enough to maintain the intended 10ms runnable and transport pacing.
- Use bounded queues/ring buffers and check transmission confirmation/error state; `LabTransmit` returning 0 means accepted for transmission, **not** necessarily ACKed on wire.

A real AUTOSAR BSW integration should replace the teaching RTE/communication/diagnostic stack with generated configuration and compliant modules, instead of directly mixing with vendor HAL callbacks.

## Firmware toolchains / debugger

**H723:** STM32CubeH7 HAL + CubeMX project, arm-none-eabi-gcc, J-Link or OpenOCD as supported by your board. Consult ST's AN5348 for FDCAN initialization, shared message RAM, FDCAN filters, bit timing and DLC mapping:
https://www.st.com/content/st_com/en/technical-documents/an5348-introduction-to-fdcan-peripherals-for-stm32-mcus-stmicroelectronics.html

**M416:** official Artery firmware library/BSP and reference manual:
https://www.arterychip.com/en/product/AT32M416.jsp
J-Link/AT-Link support depends on probe firmware and board; verify actual SWD wiring and reset pins against board schematics.

For either target:
- Verify SWD probe attaches to the exact CPU and halted PC points to valid startup code.
- After vendor project links, use `arm-none-eabi-size firmware.elf` and inspect `.map`.
- Add read-only diagnostics: UART firmware version/hash, task count, CAN RX/TX counts, lost frames, bus-off state.
- Preserve a known-good firmware and reliable SWD recovery method.

## Test plan and acceptance gates

| Stage | Gate |
| --- | --- |
| CPU/SWD | Deterministic LED/UART after cold boot and reset |
| CAN Classical | Independent RX, TX + ACK at 500 kbit/s without bus errors |
| CAN FD | 16-byte 0x201 BRS frame, 2 Mbit/s data phase, compatible ISO FD |
| Portable core | Velocity payload matches 0x201 definition on wire |
| Diagnostic | 0x10, 0x22 F187, 0x22 F190 via external tester, FC/STmin works |
| Stability | Bounded queue, no unbounded heap, faults recovered, watchdog tested |
| OS/BSW | Only after validated third-party or vendor AUTOSAR integration on target |

See `hardware/stm32h723/README.md` and `hardware/at32m416/README.md` for board-specific checklists.
