# STM32H723: main ECU board bring-up

**Status:** MCU not connected to this repo's host for flash/validation. Board-specific pinout unknown. Core C cross compilation: verified Cortex-M7 (object-only).

## Dependencies

- STM32CubeH7 HAL, STM32CubeMX/IDE-generated project for the exact STM32H723 package and PCB.
- ARM bare-metal GCC, correct linker script and startup, SWD probe.
- FDCAN-compatible external transceiver with correct VIO/supply and 120Ω termination at physical bus end.
- Second normal-mode CAN FD-capable node (AT32M416) or an isolated bus analyzer.

## Bring-up checklist

1. Document PCB revision, crystal, SYSCLK, FDCAN kernel-clock source, TX/RX pins/alternate functions, transceiver enable/standby GPIO, power rails.
2. Boot LED/UART example from vendor. Inspect linker map, stack/memory map and verify SWD recoverability.
3. Use CubeMX to configure one FDCAN instance in **Classic CAN** normal mode: nominal 500 kbit/s, standard 11-bit filters for 0x7E0/0x201/0x7E8; check global filter and valid message-RAM layout.
4. Transmit 0x201, then receive 0x7E0. Check TX FIFO/Queue and RX FIFO callbacks, error counters, timer task timing.
5. Switch to ISO CAN FD with BRS (nominal 500k, data 2M initially). Send DLC=16 0x201; ensure code maps 16 bytes to correct DLC rather than assuming DLC equals raw byte count in HAL header.
6. Add `src/core/lab_*.c` to board build and adapt `LabTransmit` / `LabFrame`. Schedule `lab_tick` using elapsed monotonic milliseconds.
7. Move RX interrupt data into a bounded queue and call `lab_on_frame` from a task rather than complex processing in ISR.
8. Confirm external tester sees same UDS responses as virtual lab, including VIN FF/FC/CF. Collect latency, queue watermark and error-state data.

## Source layout and interface

~~~c
/* Pseudocode showing integration points, NOT a complete HAL firmware image. */
static LabEcu ecu;
static int board_tx(void *ctx, const LabFrame *frame) {
    /* Map frame->id, frame->len, frame->is_fd, frame->brs
       to HAL_FDCAN_* TX header/data, enqueue, return status. */
}
void application_init(void) { lab_init(&ecu, board_tx, NULL, false); }
void application_loop(uint32_t uptime_ms) {
    /* Drain bounded FDCAN RX queue, call lab_on_frame(&ecu, &rx); */
    lab_tick(&ecu, uptime_ms);
}
~~~

The sketch deliberately does not compile until real HAL types, startup code and pin configuration are included. Do not commit the vendor SDK into this repository. For a genuine AUTOSAR ECU replace teaching infrastructure with configured compatible RTE/BSW/MCAL.

Primary reference: [ST AN5348](https://www.st.com/content/st_com/en/technical-documents/an5348-introduction-to-fdcan-peripherals-for-stm32-mcus-stmicroelectronics.html).
