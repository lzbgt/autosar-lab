# 15 — Verify ISO-TP Flow Control timeouts and recovery on a virtual STM32

This lab extends the [real embedded diagnostic stack](14-real-dcm-mcu.md). The earlier successful test proved that the **CAN Transport Layer (CanTp)** could send a 13-byte diagnostic response. This exercise deliberately prevents the server from processing the tester's Flow Control frame, then measures the real timeout and demonstrates a subsequent successful diagnostic request **on the same server without restarting it**.

The processor is an emulated STM32H743 Cortex-M7 under Renode. Actual, unchanged AutoAS **CAN Interface (CanIf)**, **CAN Transport Layer (CanTp)**, **Protocol Data Unit Router (PduR)** and **Diagnostic Communication Manager (Dcm)** implementations run in its ARM firmware. This is not a Python-only protocol simulator.

## 1. Execute both failure and recovery experiments

~~~sh
cd ~/work/autosar-lab
bash tools/run-h743-autoas-fault.sh
~~~

The command builds a separate test-only server ELF, using the already pinned open-source AutoAS modules, and executes **two distinct experiments**. It does not modify the successful normal firmware or any file in `third_party/autoas/`.

| Experiment | What actually happens | Required outcome |
| --- | --- | --- |
| Missing Flow Control | A tester requests Data Identifier (DID) `F1 5B`. The server emits a First Frame, but the server's driver discards the received Flow Control frame before forwarding it to the CAN Interface (CanIf). | The true AutoAS transport waits `N_Bs`, aborts the multi-frame response after approximately 1,000 ms, and the Diagnostic Communication Manager (Dcm) releases its transmit buffer |
| Recovery on the same server | After the first timeout, the emulator creates a **second Cortex-M7 tester** on the same CAN hub; the server is not rebooted or reinitialized. The new tester requests extended diagnostic session `10 03`. | The actual Diagnostic Communication Manager (Dcm) responds `50 03 00 32 01 F4`, and the second tester validates all six response bytes |

The existing successful fingerprint, session-control and negative-response tests remain separate, runnable through `bash tools/run-h743-autoas-dcm.sh`.

## 2. Why the first diagnostic message needs Flow Control

The first request contains three bytes of **Unified Diagnostic Services (UDS)** payload:

~~~text
CAN identifier 0x731:
  03 22 F1 5B 00 00 00 00  ← Single Frame, payload length 3

CAN identifier 0x732:
  10 0D 62 F1 5B A5 A5 A5  ← First Frame, total response length 13

CAN identifier 0x731:
  30 00 00 00 00 00 00 00  ← Flow Control: Continue To Send
~~~

The reply is 13 bytes: three bytes `62 F1 5B` followed by ten `A5` bytes. Classical Controller Area Network (CAN) cannot fit it in one eight-byte data frame. The server sends a **First Frame (FF)** and must wait for the tester's **Flow Control frame (FC)** before sending any **Consecutive Frame (CF)**.

With normal operation, the server sends a Consecutive Frame holding the remaining seven bytes. In this test, the server's Bosch M_CAN model **has already received** the Flow Control frame into its receive FIFO, but `hardware/simulator/h743-autoas/board_mcan.c` suppresses the call to `CanIf_RxIndication()` for that frame in a separately compiled test variant. The receive FIFO entry is acknowledged and released.

This is a controlled **driver-to-communication-stack frame-loss simulation**, not an electrical bus-loss simulation. The emulated CAN hub successfully delivered the frame; it is intentionally withheld from the communication stack after receipt. The normal driver has no such interception.

## 3. Derive the timeout from configured units

The test server configures:

- `N_Bs = 100` in `hardware/simulator/h743-autoas-dcm/dcm_server.c`, expressed as 100 invocations of `CanTp_MainFunction()`.
- The main function runs every **10 ms** under a real emulated Cortex-M7 SysTick interrupt schedule, as verified in [chapter 14](14-real-dcm-mcu.md).

Therefore:

\[
t_{N_{Bs}} = 100\ \text{cycles} \times 10\ \text{ms/cycle} = 1000\ \text{ms}.
\]

The tested ARM firmware observed:

| Independent CPU observation | Measured value |
| --- | ---: |
| Timestamp when real CAN Transport Layer (CanTp) entered `CANTP_WAIT_FIRST_FC` | 10 ms |
| Timestamp when real CAN Transport Layer (CanTp) returned to `CANTP_IDLE` | 1,010 ms |
| Elapsed real virtual-time ticks | **1,000 ms** |
| Raw Flow Control frames received by the M_CAN model | 1 |
| Flow Control frames deliberately withheld from the CAN Interface (CanIf) | 1 |
| Consecutive Frames transmitted during failed response | 0 |
| Diagnostic Communication Manager (Dcm) transmit buffer recovered to idle | Yes |

The test records these milestones from actual Cortex-M7 Static Random-Access Memory (SRAM) diagnostic words. The observed timeout is produced by the unmodified AutoAS transport state machine, not a test-harness timer that directly sets a timeout flag.

The server success marker after the first abort is `0xFA17FC00`, representing **fault detected and transport cleaned up**, *not* a positive diagnostic response. The original tester must **not** report a completed 13-byte response.

## 4. Prove a later service still works

After 1.15 seconds of simulated time, the recovery scenario attaches a second STM32H743 model running an existing real AutoAS diagnostic tester. It sends:

~~~text
CAN identifier 0x731: 02 10 03 00 00 00 00 00
CAN identifier 0x732: 06 50 03 00 32 01 F4 00
~~~

The same server that lost the earlier Flow Control frame responds normally, without a server reset.

The independent observations are:

| Observable result | Expected status |
| --- | --- |
| First tester did not accept incomplete fingerprint response | No `0xCAFE220D` completion |
| Same-server abort then later service completion | `0xFA17C003` |
| New tester validated positive response to `10 03` | `0xCAFE1003` |
| Actual server fingerprint callback executions | 1 |
| Actual server diagnostic session-change callback executions | 1 |
| Actual server physical transmit confirmations | 2: failed First Frame plus successful later Single Frame |
| Recorded Flow Control drop and transport timeout | One drop; elapsed 1,000 ms |
| Renode emulator errors | 0 |

The second test uses a new *tester*, but retains the existing server process, its memory, its diagnostic module state and its CAN controller. It therefore checks service restoration, not success after silently rebooting the target.

## 5. Inspect and deliberately challenge the verifier

The test script `tools/test-autoas-fc-loss.py` reads 28 emulated SRAM words for the timeout-only experiment, then 39 independent words for the timeout-followed-by-recovery experiment. It checks observed frame types, actual startup and timeout timestamps, SysTick counters, module transmit confirmations, diagnostic callback counts and final states.

Verify the checks independently of the emulator:

~~~sh
python3 tools/test-autoas-fc-loss.py --self-test
~~~

The validator deliberately changes valid evidence into invalid data and must reject false successful diagnostic responses, missing Flow Control drops, incorrect timeout duration, wrong hardware-clock cadence, missing diagnostic recovery and other corruptions. A zero-exit Renode process alone is **not enough** to pass.

Raw emulator output and generated scenarios are stored under the Git-ignored `build/emu/h743-autoas-dcm/` directory:

~~~text
fc-loss.log      one request, dropped FC, real N_Bs timeout
fc-recovery.log  first timeout, then later successful 10 03 on same server
fc-loss.resc     generated Renode scenario
fc-recovery.resc generated sequential-tester Renode scenario
~~~

## 6. Limits and next stages

**Proved:** the real AutoAS CAN Transport Layer (CanTp) times out and resets after missing Flow Control; the real Diagnostic Communication Manager (Dcm) releases its transmit buffer; and the same firmware later responds to a valid diagnostic session request. The **one-millisecond SysTick** and **ten-millisecond Basic Software (BSW)** scheduling cadence remained correct in virtual time.

**Not proved:** electrical CAN arbitration, transceiver voltage, real bus-off recovery, CAN FD data-phase timing, worst-case execution time on physical hardware, or readiness for the exact STM32H723 and AT32M416 devices. This test is not an AUTOSAR conformance evaluation.

The next useful work is a genuine Trampoline Operating System (OS) integration on the same MCU, followed by watchdog/fault-policy validation, then physical two-node CAN FD tests with external transceivers and an analyzer. No source in the pinned upstream AutoAS checkout has been modified; its GPL version 3/commercial licensing remains applicable.
