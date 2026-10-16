# 04 — Run the virtual ECU laboratory

## L0: host build and deterministic unit tests

~~~sh
cd ~/work/autosar-lab
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
~~~

Expected: `core_protocol` passed. This verifies time-based signal updates, UDS single-frame/negative responses, FF/FC/CF segmentation, timeout cleanup, transmission error counting and FD telemetry framing **without kernel CAN modules or root**.

## L1: three-terminal rootless bus (use this on the current Arch kernel)

Terminal A — frame relay:

~~~sh
python3 tools/virtual_bus.py --verbose
~~~

Terminal B — hosted virtual ECU:

~~~sh
./build/lab_ecu --backend udp --port 18765 --verbose
~~~

Terminal C — UDS tester:

~~~sh
python3 tools/uds.py --backend udp --session 3
python3 tools/uds.py --backend udp --did 0xF187
python3 tools/uds.py --backend udp --did 0xF190 --block-size 1
python3 tools/uds.py --backend udp --did 0xF190 --block-size 0
python3 tools/uds.py --backend udp --did 0xF199 --expect-negative 0x31
~~~

ECU should transmit CAN ID 0x201 every ~100 ms (host scheduling jitter applies). The tester sends 0x7E0 and observes responses on 0x7E8; F190 returns dummy VIN **LAB00000000000001** using ISO-TP FF, FC and CF.

## L2: one-command end-to-end test

~~~sh
python3 tools/smoke.py --backend udp --build-dir build
python3 tools/smoke.py --backend udp --fd --build-dir build
~~~

The smoke runner creates and cleans up its own relay and ECU, subscribes before launch to verify telemetry, and runs diagnostic probes. `--fd` changes **only the periodic speed message** to a 16-byte FD payload with BRS flag; diagnostics remain Classical CAN frames.

Do not run two relay instances on the same port simultaneously. When running the manual lab while testing, choose a different port:

~~~sh
python3 tools/virtual_bus.py --port 18766
./build/lab_ecu --backend udp --port 18766
python3 tools/uds.py --backend udp --port 18766 --did 0xF190
~~~

The UDP relay is unprivileged and isolated to IPv4 loopback. It forwards 72-byte frames but **does not model bus electrical faults, CAN ACK, arbitration, retransmissions, bit stuffing, error counters or bus-off**. Its framing is a custom lab protocol documented in `src/platform/lab_transport.c`; it is not ISO-TP-over-UDP or a substitute for a vehicle bus.

## L3: replace transport with SocketCAN vcan

If the running kernel supports vcan:

~~~sh
bash tools/setup-vcan.sh vcan0
./build/lab_ecu --backend socketcan --ifname vcan0 --verbose
# in a second terminal
python3 tools/uds.py --backend socketcan --ifname vcan0 --did 0xF190
# or use the orchestrated smoke test
python3 tools/smoke.py --backend socketcan --ifname vcan0
~~~

On systems with can-utils:

~~~sh
candump vcan0
cansend vcan0 7E0#0322F19000000000
~~~

**Current Arch host finding (2026-10-16):** `modinfo vcan` returned "Module vcan not found" and `sudo ip link add dev vcan0 type vcan` returned "Unknown device type". Do **not** reboot the machine merely for this lab. vcan may become available after installing modules matched to the **running** kernel (and rebooting only with owner awareness), or use the rootless relay.

`vcan` also does not represent physical arbitration/ACK behavior. It is Linux's simulated CAN network interface, useful for exercising SocketCAN code.

## L4: make and measure a change

1. Predict first payload: 5 cm/s per 10ms runnable, ten activations at 100ms → 50 cm/s = 0x0032.
2. Change step 5 to 7 in `lab_application_run_10ms`.
3. Build, run ECU with `--verbose` and inspect TX 0x201.
4. Re-run CTest; adjust/revert the deterministic expected value in `test_core.c` for your experiment.
5. Restore original code with Git diff and rerun tests.

## L5: learn real ISO-TP by inspecting code

Trace `src/core/lab_cantp.c`:
- `lab_cantp_transmit`: length <=7 → SF, else emits FF and waits for FC.
- `lab_cantp_rx_indication`: tester's FC CTS (0x30) selects block size/STmin.
- `lab_cantp_main_function`: sends CF 0x21, 0x22... as allowed by BS and time.
- `lab_dcm_request`: chooses UDS positive/negative payload.

Experiment: change the tester's `--block-size` to 2 or 0. No ECU source change should be required. Describe the number of FC transmissions.

## Test outcomes that do NOT prove compliance

The passes prove exactly the educational behaviors stated above. They do not prove automotive RTOS scheduling, MCAL suitability, AUTOSAR API compliance, ISO-TP conformance, functional safety, CAN FD physical timing, electromagnetic compatibility or diagnostic tester interoperability.
