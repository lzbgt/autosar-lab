# 06 — Study an authentic Operating System (OS) and open Basic Software (BSW) without paid tooling

Our small tutorial Electronic Control Unit (ECU) intentionally uses **handwritten** interfaces. To understand real configuration and upstream code, study these **separate** implementations. Never call the tutorial stack AUTOSAR-conformant.

## A. Trampoline: real OSEK/AUTOSAR Operating System (OS)-aligned Application Programming Interfaces (APIs)

Project: https://github.com/TrampolineRTOS/trampoline
License: GPL-2.0. Portable Operating System Interface (POSIX) target for Linux; OSEK Implementation Language (OIL)/Trampoline configuration generator (GOIL) generated configuration; Cortex-M4 STM32F407 Discovery examples. Its documentation describes alignment with OSEK/VDX and AUTOSAR Operating System (OS) 4.2.

Fetch the exact revision pinned on project creation:

~~~sh
cd ~/work/autosar-lab
bash tools/fetch-upstreams.sh trampoline
git -C third_party/trampoline rev-parse HEAD
# expected: ff287023252bf73273067f00d2dbc8bbc9a20401
~~~

Source kept in ignored `third_party/`. This prevents GPL-licensed Operating System (OS) code being bundled into MIT example sources or confusing Git histories. If cloning fails, verify internet/proxy and retry the script with the same revision; it does not overwrite a different local checkout.

### Portable Operating System Interface (POSIX) scheduling exercise

Inspect:

~~~sh
find third_party/trampoline/examples/posix -maxdepth 2 \
  -name '*.oil' -o -name 'README*' | head -25
ls third_party/trampoline/goil/templates
~~~

The pinned Trampoline revision has been built and executed successfully on the Arch host. Reproduce the real Operating System (OS) exercise with:

~~~sh
cd ~/work/autosar-lab
bash tools/run-trampoline-posix.sh
~~~

This script builds the Trampoline configuration generator (GOIL) and ViPER if required, invokes Trampoline configuration generator (GOIL) with `--target=posix/linux`, compiles the periodic Operating System (OS) task and checks output for activations and an orderly shutdown. It operates entirely inside the ignored `third_party/` checkout and is independent of the teaching Electronic Control Unit (ECU).

What to inspect:
- `periodic.oil`: TASK, COUNTER, ALARM, APPMODE and their attributes.
- Generated C/headers: task IDs, timing structures, Operating System (OS) configuration.
- Periodic runnable execution and `ActivateTask`/`TerminateTask` calls.
- Scheduling behavior and preemption compared with our simple `lab_tick`.

A successful Trampoline demo proves actual Operating System (OS) code executed **on Portable Operating System Interface (POSIX)**. It does not establish AUTOSAR Basic Software (BSW) compatibility or STM32H723 suitability; the upstream published STM32 example is for F407.

## B. AutoAS / AS: separate Basic Software (BSW) study and diagnostics

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

Upstream documents a Controller Area Network (CAN) diagnostic simulation and commands similar to:

~~~text
scons --app=CanApp
scons --app=IsoTpSend
~~~

The documented environment is Windows/MSYS2, and the example's exact build path and platform dependencies must be checked before attempting native Arch builds. It is **not** a drop-in AUTOSAR 4.x Microcontroller Unit (MCU) deployment for H723/M416, nor is its native Linux build guaranteed.

Trace a Unified Diagnostic Services (UDS) 0x10 request in these sources: simulator Controller Area Network (CAN) driver → CAN Interface (CanIf) → CAN Transport Layer (CanTp) → Protocol Data Unit Router (PduR) → Diagnostic Communication Manager (Dcm); then trace 0x50 response. Compare with small teaching functions in `src/core` and identify what we omitted: configuration callbacks, memory handling, state machines, development errors, Electronic Control Unit (ECU) lifecycle and thread protection.

## C. AUTOSAR XML (ARXML) and toolchain literacy

- AUTOSAR XML (ARXML) is a standardized XML description exchange format, with versioned schemas and semantic constraints. Software Components (SWCs), Electronic Control Unit (ECU) mapping, Electronic Control Unit Configuration (ECUC), transport network, timing and hardware configuration all matter; not every AUTOSAR XML (ARXML) file contains every description.
- A **Runtime Environment (RTE) generator** reads the appropriate mappings and generates Application Programming Interfaces (APIs) and binding. Handwriting functions called `Rte_Read` does not reproduce that process.
- **Trampoline configuration generator (GOIL)/OSEK Implementation Language (OIL)** is an OSEK/Operating System (OS) configuration exercise, not a full commercial AUTOSAR integrated toolchain.
- **Vector DaVinci / EB tresos** and vendor Microcontroller Abstraction Layer (MCAL) are commonly used in commercial Classic integration; they are not required for architectural training. Their generated artifacts/licensing are a separate workflow.
- **AUTOSAR specifications** are available to read at https://www.autosar.org/standards/classic-platform/ ; the official site includes IP/copyright notice. Do not bulk-copy specifications into this Git repository.

## Verify the distinctions in writing

Draw two diagrams: (1) this repo's handwritten dispatcher and UDP backend, (2) Trampoline's generated Operating System (OS) config and task scheduler. List exactly which components would need replacement or integration to build a real Classic Electronic Control Unit (ECU). Your learning report should not simply say "AUTOSAR works" based on a Unified Diagnostic Services (UDS) echo test.

## Optional alternatives

- For Adaptive/service-oriented learning, inspect COVESA vsomeip (SOME/IP), but it is not a complete Adaptive AUTOSAR stack.
- For real Microcontroller Unit (MCU) Controller Area Network (CAN) application examples, begin with each vendor's licensed Hardware Abstraction Layer (HAL) example before integrating any third-party Operating System (OS)/Basic Software (BSW).

Source docs: https://github.com/TrampolineRTOS/trampoline and https://github.com/autoas/as
