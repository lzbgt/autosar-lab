# 05 — Development toolchain, compilation, debugging and deployment

## Host toolchain on Arch Linux

The already validated host has GCC, CMake, Ninja, Python, Git and ARM GCC. To provision a different Arch installation, install **only missing packages**:

~~~sh
sudo pacman -S --needed base-devel git gcc cmake ninja python \
  gdb clang arm-none-eabi-gcc arm-none-eabi-newlib
# Optional for real SocketCAN capture and debug probes:
sudo pacman -S --needed can-utils openocd
~~~

GCC compiles ordinary POSIX C for the virtual ECU; `arm-none-eabi-gcc` compiles freestanding code for Cortex-M targets. The latter is **not** a vendor driver library, linker script or startup implementation. A genuine MCU image also needs vector table, system clock, board initialization, HAL/MCAL and memory map.

## Complete host build from a clean checkout

~~~sh
cd ~/work/autosar-lab
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 -m compileall -q tools
python3 tools/smoke.py --backend udp --build-dir build
python3 tools/smoke.py --backend udp --fd --build-dir build
bash tools/check-arm-core.sh
~~~

Build outputs go only into ignored `build/`. Both host and target compilation use strict warnings for portable core code. Avoid committing generated objects, toolchains, copied SDK trees and credentials.

### What each command proves

| Command | Evidence | Does *not* prove |
| --- | --- | --- |
| CMake build | Linux source and link are valid | CAN wire functionality |
| CTest | Deterministic core transitions pass | Network integration |
| UDP smoke | Periodic frame transport + diagnostics on two processes | CAN electrical arbitration |
| Optional SocketCAN smoke | Real Linux CAN socket interface works | Physical bus timing |
| ARM compile-only | Portable C source compiles for M4/M7 instruction sets | Firmware image boots |
| SWD flash + scope trace | MCU software runs and transceiver communicates | AUTOSAR compliance or functional safety |

## Linux debugger: test C code

Run bus separately:

~~~sh
python3 tools/virtual_bus.py --port 18766
~~~

Then in another terminal:

~~~sh
gdb --args ./build/lab_ecu --backend udp --port 18766 --verbose
~~~

Inside GDB:

~~~text
set pagination off
break lab_dcm_request
break lab_cantp_transmit
break lab_cantp_main_function
run
~~~

In a third terminal send:

~~~sh
python3 tools/uds.py --backend udp --port 18766 --did 0xF190
~~~

Inspect arguments (`print *ecu`, `x/20bx request`), step into response routing, examine `tp_length`, `tp_sent` and `tp_wait_flow`. Print `backtrace` on errors. Add GDB watchpoints to diagnose state changes. Use `--verbose` on the ECU to dump outgoing frames; the third-party bus relay's `--verbose` also prints IDs and DLCs.

Run sanitizer build when changing transport code:

~~~sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
python3 tools/smoke.py --backend udp --build-dir build-asan
~~~

Host tests are deliberately free of privileged network operations. If the compiler supports it, also run `clang-tidy` and your project's static-analysis/CI policies.

## Cross compilation, linking and what changes for MCUs

The compilation pipeline is:

~~~text
.c/.h → C preprocessor → compiler → .o objects
                              ↓
             startup.o + vendor HAL + portable application.o
                              ↓
         linker script (.ld) + linker → .elf with vector table
                              ↓
          objcopy (.bin/.hex) → flash via SWD/bootloader
                              ↓
                  reset → serial/SWO log + CAN trace
~~~

`bash tools/check-arm-core.sh` uses Cortex-M7 and M4 builds and stops at `.o`. It must never be misrepresented as generating bootable images.

For a real STM32H723 or AT32M416 image:

1. Start from the **exact device/board vendor project** with startup, clock, RAM/Flash map and CAN/FDCAN driver support.
2. Add `src/core/lab_*.c` and `src/core/lab.h` to the firmware build.
3. Implement `LabTransmit` using the vendor's CAN TX HAL and translate RX frames into `LabFrame`.
4. Invoke `lab_tick(&ecu, uptime_ms)` at a sufficiently regular rate and feed `lab_on_frame` from a bounded receive queue.
5. Enable watchdog, bounds checking and health reports. Replace teaching BSW with real configured modules when moving to an AUTOSAR-targeted integration.

Typical tools:
- STM32: STM32CubeMX / STM32CubeIDE, STM32CubeH7 HAL, OpenOCD or J-Link, arm-none-eabi-gdb.
- AT32M416: vendor firmware library/BSP, a supporting SWD probe (J-Link or AT-Link), GNU Arm or other supported compiler.
- Wire capture: CAN FD-capable USB analyzer or a third MCU/board as a **listen-only** monitor; logic analyzer sees only MCU UART/SPI/GPIO unless suitable CAN decoding/transceiver attached.
- ECU integration: CAN trace/logger, UDS client, test scripts and eventually HIL.

## A safe firmware deployment and rollback sequence

1. Flash a vendor **LED/UART-only** example; verify clock, debugger attach, reset behavior and voltage.
2. Back up the original bootloader/flash **if applicable and permitted**; record firmware hash/build ID.
3. Compile a CAN RX-only listener, then test Classic frames.
4. Add bounded TX; verify with external monitor while wheels/motors are **unpowered**.
5. Turn on CAN FD only after both controllers/transceivers are compatible.
6. Bring in the portable core application and diagnostics, verify response latency and watchdog behavior.
7. Keep a known-good SWD recovery path; avoid automatic erase-all or bootloader modification unless needed.

## Practical performance measurements

- Runnable timing: scheduled activation versus actual start, WCET and missed deadlines.
- CAN traffic: ID, length, nominal/data bitrate, frame period/jitter, error counters, dropped RX frames.
- Diagnostic: request→response elapsed latency (P2/P2*), FF→FC delay, CF STmin/BS policy.
- Resource footprint: Flash text/rodata, SRAM data/bss/stack/heap, queue high-watermarks.
- Fault tests: missing ACK, bus-off, disconnected transceiver, duplicate CAN ID, malformed ISO-TP FC.

**Exit gate:** reproducible build + CTest + UDS smoke now, then actual SWD/physical trace and quantitative evidence when boards are connected.
