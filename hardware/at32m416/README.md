# AT32M416CBT7: peer Controller Area Network Flexible Data-Rate (CAN FD) Electronic Control Unit (ECU) bring-up

**Status:** Microcontroller Unit (MCU) not connected/board not flashed in this repo. Verified portable source compiles for Cortex-M4 (object-only). The exact package AT32M416CBT7 has 128KB Flash and 16KB Static Random-Access Memory (SRAM) with **one Controller Area Network Flexible Data-Rate (CAN FD) controller** per [official Artery documentation](https://www.arterychip.com/en/product/AT32M416.jsp).

## Key chip-specific constraints

- Cortex-M4F class, 180 MHz maximum core frequency (dependent on supply and configuration).
- One Controller Area Network Flexible Data-Rate (CAN FD) controller supporting Classical frames and Controller Area Network Flexible Data-Rate (CAN FD); do not assume exactly the same peripheral register/interface model as STM32's Bosch M_CAN FDCAN.
- Vendor datasheet states the Controller Area Network (CAN) peripheral's clock source must come from the **HEXT-based Phase-Locked Loop (PLL)** for Controller Area Network (CAN) transmission quality.
- 16KB Static Random-Access Memory (SRAM) is constrained: avoid full-size debug log buffers, unbounded queues and large diagnostic allocations.
- The H723's STM32 Hardware Abstraction Layer (HAL) functions cannot be compiled into the AT32 firmware; adapt to the **Artery firmware library** Application Programming Interface (API).

## Minimal milestones

1. Obtain board schematic and vendor BSP for the exact AT32M416CBT7 PCB. List oscillator, 3.3V supplies, CANFD Transmit (TX)/Receive (RX) pin mapping and transceiver standby control.
2. Verify basic LED/Universal Asynchronous Receiver/Transmitter (UART) program, Serial Wire Debug (SWD) attach, reset and full recovery path before editing existing flash partitions/bootloader.
3. Start a standalone Artery Controller Area Network (CAN) driver example in Classical Controller Area Network (CAN) 500 kbit/s, standard ID filter and normal mode.
4. Add external FD-compatible transceiver; connect twisted CAN_H/L + common GND to H723; two physical terminations.
5. Verify independent transmission/receive, Acknowledgment (ACK), recovery and error status in both directions.
6. Enable ISO Controller Area Network Flexible Data-Rate (CAN FD) and 16-byte Bit Rate Switch (BRS) frame (nominal 500k/data 2M) after verifying exact CANFD clock and timing.
7. Add portable `src/core` modules; implement `LabTransmit` mapping and Receive (RX) queue; profile flash/Static Random-Access Memory (SRAM), IRQ latency and timeout behavior.
8. Test the M416 as Controller Area Network (CAN) signal publisher while H723 handles diagnostics. Observe frames from a third independent listener if available.

Vendor documents: [AT32M416 resource page](https://www.arterychip.com/en/product/AT32M416.jsp) and the linked **datasheet**, **reference manual**, firmware library BSP and AN0229 getting-started guide. SEGGER also publishes an AT-START-M416 J-Link setup article; check board revision rather than copying demo Serial Wire Debug (SWD) header pins blindly.

## Why a peer Microcontroller Unit (MCU) instead of running two full stacks?

One constrained Microcontroller Unit (MCU) can focus on Controller Area Network (CAN) and a small Software Component (SWC)/driver while H723 carries the larger stack. Once the wire and diagnostics are proven, gradually increase module coverage on M416 and measure memory/determinism. Never assume a commercial AUTOSAR 4.x Basic Software (BSW) will fit 16KB Static Random-Access Memory (SRAM) without configuration and footprint analysis.
