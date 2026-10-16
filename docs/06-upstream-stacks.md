# 06 — Study an authentic OS and open BSW without paid tooling

Our small tutorial ECU intentionally uses **handwritten** interfaces. To understand real configuration and upstream code, study these **separate** implementations. Never call the tutorial stack AUTOSAR-conformant.

## A. Trampoline: real OSEK/AUTOSAR OS-aligned APIs

Project: https://github.com/TrampolineRTOS/trampoline
License: GPL-2.0. POSIX target for Linux; OIL/GOIL generated configuration; Cortex-M4 STM32F407 Discovery examples. Its documentation describes alignment with OSEK/VDX and AUTOSAR OS 4.2.

Fetch the exact revision pinned on project creation:

~~~sh
cd ~/work/autosar-lab
bash tools/fetch-upstreams.sh trampoline
git -C third_party/trampoline rev-parse HEAD
# expected: ff287023252bf73273067f00d2dbc8bbc9a20401
~~~

Source kept in ignored `third_party/`. This prevents GPL-licensed OS code being bundled into MIT example sources or confusing Git histories. If cloning fails, verify internet/proxy and retry the script with the same revision; it does not overwrite a different local checkout.

### POSIX scheduling exercise

Inspect:

~~~sh
find third_party/trampoline/examples/posix -maxdepth 2 \
  -name '*.oil' -o -name 'README*' | head -25
ls third_party/trampoline/goil/templates
~~~

The pinned Trampoline revision has been built and executed successfully on the Arch host. Reproduce the real OS exercise with:

~~~sh
cd ~/work/autosar-lab
bash tools/run-trampoline-posix.sh
~~~

This script builds the GOIL generator and ViPER if required, invokes GOIL with `--target=posix/linux`, compiles the periodic OS task and checks output for activations and an orderly shutdown. It operates entirely inside the ignored `third_party/` checkout and is independent of the teaching ECU.

What to inspect:
- `periodic.oil`: TASK, COUNTER, ALARM, APPMODE and their attributes.
- Generated C/headers: task IDs, timing structures, OS configuration.
- Periodic runnable execution and `ActivateTask`/`TerminateTask` calls.
- Scheduling behavior and preemption compared with our simple `lab_tick`.

A successful Trampoline demo proves actual OS code executed **on POSIX**. It does not establish AUTOSAR BSW compatibility or STM32H723 suitability; the upstream published STM32 example is for F407.

## B. AutoAS / AS: separate BSW study and diagnostics

Project: https://github.com/autoas/as
License: **GPLv3 or a separate commercial license**. GPLv3 permits commercial use if its obligations are met; an alternative commercial license can be appropriate for proprietary distribution. Read the checkout LICENSE.

~~~sh
cd ~/work/autosar-lab
bash tools/fetch-upstreams.sh autoas
git -C third_party/autoas rev-parse HEAD
# expected: 7bf0210644a932ae45e6cabbe8388729808af7fd
~~~

Inspect modules:

~~~sh
find third_party/autoas -iname '*CanIf*' -o -iname '*CanTp*' \
  -o -iname '*Dcm*' -o -iname '*PduR*' | head -60
~~~

Upstream documents a CAN diagnostic simulation and commands similar to:

~~~text
scons --app=CanApp
scons --app=IsoTpSend
~~~

The documented environment is Windows/MSYS2, and the example's exact build path and platform dependencies must be checked before attempting native Arch builds. It is **not** a drop-in AUTOSAR 4.x MCU deployment for H723/M416, nor is its native Linux build guaranteed.

Trace a UDS 0x10 request in these sources: simulator CAN driver → CanIf → CanTp → PduR → Dcm; then trace 0x50 response. Compare with small teaching functions in `src/core` and identify what we omitted: configuration callbacks, memory handling, state machines, development errors, ECU lifecycle and thread protection.

## C. ARXML and toolchain literacy

- ARXML is a standardized XML description exchange format, with versioned schemas and semantic constraints. SWCs, ECU mapping, ECUC, transport network, timing and hardware configuration all matter; not every ARXML file contains every description.
- A **RTE generator** reads the appropriate mappings and generates APIs and binding. Handwriting functions called `Rte_Read` does not reproduce that process.
- **GOIL/OIL** is an OSEK/OS configuration exercise, not a full commercial AUTOSAR integrated toolchain.
- **Vector DaVinci / EB tresos** and vendor MCAL are commonly used in commercial Classic integration; they are not required for architectural training. Their generated artifacts/licensing are a separate workflow.
- **AUTOSAR specifications** are available to read at https://www.autosar.org/standards/classic-platform/ ; the official site includes IP/copyright notice. Do not bulk-copy specifications into this Git repository.

## Verify the distinctions in writing

Draw two diagrams: (1) this repo's handwritten dispatcher and UDP backend, (2) Trampoline's generated OS config and task scheduler. List exactly which components would need replacement or integration to build a real Classic ECU. Your learning report should not simply say "AUTOSAR works" based on a UDS echo test.

## Optional alternatives

- For Adaptive/service-oriented learning, inspect COVESA vsomeip (SOME/IP), but it is not a complete Adaptive AUTOSAR stack.
- For real MCU CAN application examples, begin with each vendor's licensed HAL example before integrating any third-party OS/BSW.

Source docs: https://github.com/TrampolineRTOS/trampoline and https://github.com/autoas/as
