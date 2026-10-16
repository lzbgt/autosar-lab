# 10 — Troubleshooting: trace the fault from layer to layer

## Quick checks

~~~sh
cd ~/work/autosar-lab
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 tools/smoke.py --backend udp --build-dir build
~~~

If these pass, the portable teaching stack and rootless bus integration work. Failures on a physical bus are likely due to drivers, bit timing, wiring, filters or OS event handling rather than the tested protocol code.

## Common symptoms

| Symptom | Check | Fix / next experiment |
| --- | --- | --- |
| `modprobe: Module vcan not found` | `uname -r` and ``/usr/lib/modules/` for matching kernel | Use rootless UDP. Do not assume a module for the *installed* newer kernel matches the *running* one; don't force reboot for the lab |
| `ip link ... Unknown device type` | Kernel configuration/module actually running | Use rootless UDP; revisit matching kernel modules on later maintenance window |
| `python tools/uds.py: Connection refused` | Is `tools/virtual_bus.py` running on the chosen UDP port? | Start one relay; match ports |
| `no periodic speed frames` | ECU running? same bus, no duplicate relay? | Check ECU with `--verbose` and relay console; run smoke |
| `UDS response timeout` | ECU listening on 0x7E0; reply 0x7E8; CAN filters; tester peer registered | Trace TX / RX at each layer and raw capture; check IDs |
| `UDS negative response 7F 22 31` | Unsupported DID | Request supported F187/F190 |
| `VIN never finishes` | Did tester send FC? wrong sequence, BS/STmin, missed deadlines? | Try `--block-size 0`, inspect `lab_cantp_main_function` and tester trace |
| `CAN FD frame rejected` | Controller + kernel socket FD option, transceiver, DLC table | Try 16-byte FD on FD-only bench. Never feed FD frames to Classical-only node |
| `Tx attempted but no ACK` | At least one correctly configured **other** CAN controller in normal mode? | Add peer, verify termination and nominal bitrate |
| `Lots of errors / bus-off` | Mismatched bitrate/sample point, wiring, oscillator, transceiver standby | Stop TX, verify scope and error counters, repair before retry |
| `Expected CTest changes` | You deliberately changed application increment or response layout | Reconcile assertions with requirements; document new expected payload |
| `GOIL not found` | Third-party checkout only fetched source; generator not on PATH | Read upstream `goil/README.md`; build GOIL on Linux or use its official binary |
| `ARM cross compile works but board won't boot` | Startup, linker script, clocks, vector table, power/SWD | Return to vendor LED example and systematically re-integrate |
| `J-Link can't attach M416` | Probe supports part, SWD wiring, correct reset/voltage | Follow Artery or SEGGER AT-START-M416 board-specific instructions |

## Debugging as a pipeline

~~~text
SWC state wrong?
  └─ check runnable invocation, sensor scaling and RTE data mapping
COM payload wrong?
  └─ check byte order, bit position, length and signal conversion
PduR/CanIf missing?
  └─ check configured PDU IDs, direction and controller state
Can MCAL no TX?
  └─ inspect filter/init sequence, tx confirmation, errors and clocks
No CAN wire ACK?
  └─ two active nodes, CAN_H/L, termination, bus speed and common ground
CanTp/UDS wrong?
  └─ log SF/FF/FC/CF, session state, DID, NRC and timing parameters
~~~

Record one observation at each boundary. Debugging via a global "CAN works/doesn't work" hypothesis wastes time.

## Resource and concurrency failures on MCU

For target with 16KB RAM (M416), audit .map and runtime stack headroom. Configure all RX/TX queue depths and handle overflow deterministically; never spin indefinitely waiting for CAN TX, particularly in interrupts. Feed `lab_on_frame` from a task/queued context. Verify that `lab_tick` executes even when CAN receive is quiet: ISO-TP timeout and periodic telemetry depend on it.

If OS task priorities or preemption change, rerun jitter and queue tests; a passing POSIX simulation is not a real-time guarantee.

## Safety boundary

This repo is intended for isolated bench use. Do not send unreviewed UDS security/programming/control requests on a vehicle or production actuator system. Keep motors/actuators disabled until the telemetry and failsafe path are physically verified.
