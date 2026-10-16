# 13 — Run genuine AutoAS communication modules on virtual STM32 microcontrollers

This chapter connects **unchanged upstream AUTOSAR-oriented Basic Software (BSW)** implementations to a **real ARM firmware image**, rather than merely compiling a Linux-host version or writing functions that imitate their names.

We have now executed three real upstream modules on the Renode STM32H743 Cortex-M7 model:

- **CAN Interface (CanIf)** — validates and routes configured CAN Protocol Data Units (PDUs) between applications/transport modules and the microcontroller CAN driver. Real source: `third_party/autoas/infras/communication/CanIf/CanIf.c`.
- **CAN Transport Layer (CanTp)** — implements transport framing, segmentation, reassembly, Flow Control, timeouts and transmit confirmations according to a useful subset of ISO 15765-2. Real source: `third_party/autoas/infras/communication/CanTp/CanTp.c`.
- **Protocol Data Unit Router (PduR)** — follows configured routing paths to exchange data between transport layers and upper applications. Real sources: `third_party/autoas/infras/communication/PduR/PduR.c`, `PduR_CanTp.c` and `PduR_Dcm.c`.

The source is the pinned [AutoAS upstream checkout](https://github.com/autoas/as) `7bf0210644a932ae45e6cabbe8388729808af7fd` and has **not been modified or copied into this project's tracked source tree**. Licensing belongs to upstream AutoAS (GPL version 3 or a separate commercial license), not the MIT license used by our handwritten integration code.

## 1. One command, two distinct end-to-end experiments

~~~sh
cd ~/work/autosar-lab
bash tools/run-real-autoas-mcu.sh
~~~

Both exercises cross-compile with `arm-none-eabi-gcc`, link with a real Cortex-M7 startup vector and linker script, and run as **two independent STM32H743 emulated processors** connected through the Renode internal CAN hub. Renode was already installed and SHA-256-verified by [chapter 12](12-mcu-simulation.md); the scripts check for the pinned simulator if missing.

| Stage | Independent virtual programs | Required observations |
| --- | --- | --- |
| A: CAN Interface only | `sender.elf` and `receiver.elf` under `build/emu/h743-autoas/` | Sender 16-byte CAN FD → receiver callback → eight-byte Classical CAN response → sender callback; correct flags, payload and confirmations |
| B: Full transport/routing subset | `tester.elf` and `server.elf` under `build/emu/h743-autoas-tp/` | Actual Unified Diagnostic Services (UDS)-shaped request `22 F1 5B` and **13-byte** response `62 F1 5B` + ten `A5` bytes over real ISO-TP First/Flow Control/Consecutive Frames |

The scripts compare **actual CPU-written emulated Static Random-Access Memory (SRAM)** values and exit nonzero if a callback fails. Merely generating an ELF or successfully launching Renode does not pass the test.

### Stage A: real AutoAS CAN Interface

~~~sh
bash tools/run-h743-autoas-canif.sh
~~~

Firmware source and generated output:

- `hardware/simulator/h743-autoas/autoas_canif_app.c`: one transmit Protocol Data Unit (PDU) at identifier `0x201` on the sender and `0x202` on the receiver, one configured receive Protocol Data Unit (PDU) per processor, with full-length payload checks.
- `hardware/simulator/h743-autoas/CanIf_Cfg.h`: bounded compile-time packet configuration (one controller, no deferred message buffer pools).
- `hardware/simulator/h743-autoas/board_mcan.c`: small **target-specific driver adapter** for the Renode Bosch M_CAN register model, including transmit-buffer/message-RAM programming, receive FIFO polling, and physical-frame confirmation.
- `tools/build-h743-autoas-canif.sh`: includes the **real upstream `CanIf.c`**, not an implementation copied or rewritten locally.

Input and output: sender invokes `CanIf_Transmit()` → the actual upstream module uses `Can_Write()` → Renode's FDCAN1 delivers the frame → receiver calls `CanIf_RxIndication()`, and the real configured callback inspects sixteen bytes `10 11 ... 1F`. The receiver returns eight bytes `11 22 33 44 55 66 77 88` via real `CanIf_Transmit()`, which the sender validates.

| Receiver result | Sender result | Meaning |
| --- | --- | --- |
| `0xCAFEFD11` | `0xCAFECC09` | Real upstream CAN Interface (CanIf) transmit/receive and transmit-confirmation callbacks succeeded on both CPUs |

The CPU diagnostic counters also require **one receive callback and one transmitted-frame confirmation on each node, with zero development-error reports**. These are physical-frame observations; no transport reassembly is claimed in Stage A.

### Stage B: real AutoAS transport and routing

~~~sh
bash tools/run-h743-autoas-tp.sh
~~~

The second stage additionally compiles upstream `CanTp.c`, `PduR.c`, `PduR_CanTp.c` and `PduR_Dcm.c`.

The route from the virtual tester to the virtual server is:

~~~text
Tester ARM Cortex-M7                                Server ARM Cortex-M7
Application: request "22 F1 5B"                     Application: service handler
    │                                                  ▲
    ▼                                                  │
PduR_DcmTransmit() ─► PduR_TpTransmit()             PduR_TpRxIndication()
    │                                                  ▲
    ▼                                                  │
CanTp_Transmit()                                     CanTp_RxIndication()
    │                                                  ▲
    ▼                                                  │
CanIf_Transmit()                                     CanIf_RxIndication()
    │                                                  ▲
    ▼                                                  │
Can_Write() ─────► emulated M_CAN / CAN hub ───────► M_CAN Receive FIFO
~~~

The response follows the reverse route, with **First Frame (FF)**, tester **Flow Control frame (FC)** and server **Consecutive Frame (CF)** processing. For Classical CAN with normal 11-bit identifiers:

| Wire message | Identifier | Payload | Interpretation |
| --- | --- | --- | --- |
| Tester request | `0x731` | `03 22 F1 5B 00 00 00 00` | Three-byte Unified Diagnostic Services (UDS) request wrapped in an ISO-TP Single Frame (SF) |
| Server First Frame | `0x732` | `10 0D 62 F1 5B A5 A5 A5` | Transport declares 13 total Unified Diagnostic Services (UDS) bytes; six bytes of the response are present |
| Tester Flow Control | `0x731` | `30 00 00 00 00 00 00 00` | Continue To Send; Block Size (BS)=0; Minimum Separation Time (STmin)=0 |
| Server Consecutive Frame | `0x732` | `21 A5 A5 A5 A5 A5 A5 A5` | Remaining seven response bytes |

These frames are produced by **actual upstream CAN Transport Layer (CanTp) implementation**, not hard-coded framing in the test harness. The custom application provides only the upper-service payload and buffer callbacks.

The configuration in `hardware/simulator/h743-autoas-tp/tp_app.c` defines two real Protocol Data Unit Router (PduR) paths: an incoming `CanTp`-to-application route and an outgoing application-to-`CanTp` route. Their upstream route execution is enabled by `PduR_Config`, and the actual functions `PduR_CanTpCopyRxData()`, `PduR_CanTpRxIndication()`, `PduR_CanTpCopyTxData()` and `PduR_DcmTransmit()` are linked into both Cortex-M7 images.

| Tester result | Server result | Additional checks |
| --- | --- | --- |
| `0xCAFE220D` | `0xCAFE220E` | One fully reassembled diagnostic application Protocol Data Unit (PDU) on each side; two physical-frame confirmations per node; one completed transport transmit per node; zero development-error reports |

The complete examples and their output are under `build/emu/h743-autoas/` and `build/emu/h743-autoas-tp/`, excluded from Git.

**Timing limitation:** The minimal Cortex-M7 application currently invokes `CanTp_MainFunction_Fast()` from a polling loop and uses a loop counter for `CanTp_MainFunction()`. That proves protocol progression but **does not guarantee the nominal 1 ms/10 ms scheduling periods or validate transport timeouts in real milliseconds**. The next OS integration must replace loop-count scheduling with a real hardware tick and configured tasks.

## 2. Configuration versus application versus target-specific drivers

Three different kinds of code must not be conflated:

| Layer | Real or teaching? | Input and output |
| --- | --- | --- |
| Actual **Basic Software** `CanIf.c`, `CanTp.c`, `PduR.c` | **Real pinned upstream AutoAS** | Configured Protocol Data Unit (PDU) identifiers, transport blocks, callbacks, transmit confirmations |
| `CanIf_Cfg.h`, `CanTp_Cfg.h`, `PduR_Cfg.h`, and static route table | **Small handwritten lab configuration**, not a commercial AUTOSAR XML generator output | One controller, two CAN IDs, bounded protocol timers/response payloads, explicit RX/TX route tables |
| `board_mcan.c` | **Custom hardware adapter**, not a conformant Microcontroller Abstraction Layer (MCAL) driver | Emulated Cortex-M7 loads/stores to M_CAN registers, FIFO state and messages |
| `tp_app.c`'s `App_StartOfReception()`, `App_CopyRxData()`, and service handler | **Test-harness upper application**, not a genuine Diagnostic Communication Manager (Dcm) | Maps request `22 F1 5B` to a fixed 13-byte positive response, without security/session/DTC services |

The host AutoAS example in [chapter 11](11-real-autosar-stack.md) still contains the genuine Diagnostic Communication Manager (Dcm). In the *emulated MCU firmware*, however, the Diagnostic Communication Manager and a generated Runtime Environment (RTE) have **not** yet been integrated. Likewise, this Cortex-M7 firmware uses a polling loop, **not** the independently verified Trampoline real-time Operating System (OS).

## 3. Debugging practical failures

First build and inspect actual upstream symbols:

~~~sh
bash tools/build-h743-autoas-tp.sh
arm-none-eabi-nm build/emu/h743-autoas-tp/server.elf |
  grep -E 'CanIf_|CanTp_|PduR_'
~~~

Then run and recheck the hardware-side evidence:

~~~sh
bash tools/run-h743-autoas-tp.sh
python3 tools/check-autoas-renode.py cantp-pdur \
  build/emu/h743-autoas-tp/renode.log
~~~

The checker rejects mismatched response data or transport callbacks even if the emulation terminates normally. Start with the physical CAN identifier and payload length in the receive header, then trace `CanIf_RxIndication()`, `CanTp_RxIndication()`, `PduR_StartOfReception()`, application data-copy callbacks, transport confirmation, and hardware M_CAN transmit-complete state.

Because this example is compiled for Cortex-M7, you can use `arm-none-eabi-objdump` to trace real machine instructions, or Renode's GDB remote interface to stop at the real upstream `PduR_TpTransmit` or `CanTp_SendFF` symbols.

## 4. What is still required before deploying to real hardware?

This is a verified **three-module embedded Basic Software communication integration**, not a complete production AUTOSAR Electronic Control Unit (ECU).

The remaining work includes: real Diagnostic Communication Manager (Dcm) linkage and configuration; an actual generated Runtime Environment (RTE) connection to a Software Component (SWC); an integrated Trampoline Operating System (OS) task/event/timing configuration; complete CAN-driver error handling and transmit queues; and exact STM32H723/AT32M416 pin, clock and peripheral configurations. The Renode model logs nominal/data bit-timing writes as partially unhandled, so **500 kbit/s arbitration or 2 Mbit/s data-phase electrical timing has not been measured**.

A physical STM32H723 + AT32M416 CAN FD network remains the final independent verification gate, with external transceivers, termination and a CAN FD-capable analyzer. Refer to [hardware bring-up](07-mcu-bringup.md).

**License:** AutoAS is dual-licensed GPLv3/commercial. The GNU GPLv3 obligations apply to distribution of combined binaries using the upstream code under that license. Neither our separate MIT integration harness nor Renode execution changes that.
