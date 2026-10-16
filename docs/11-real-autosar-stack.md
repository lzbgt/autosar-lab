# 11 — Compile and run the real AUTOSAR-oriented software stack

This is the **real Basic Software (BSW)** exercise that the earlier virtual
learning example lacked. It uses the pinned upstream AutoAS implementation for
communication and diagnostics on Linux, with a separate real Trampoline
Operating System (OS) exercise. Do not confuse it with `src/core/`.

## Step 1 — Install the actual stack and generator toolchain

~~~sh
cd ~/work/autosar-lab
bash tools/setup-autoas.sh
bash tools/stack-status.sh
~~~

`setup-autoas.sh` checks out AutoAS source at pinned revision
`7bf0210644a932ae45e6cabbe8388729808af7fd` and installs a local,
isolated Python toolchain (`.venv-autoas/bin/scons` plus generators).
The `stacks/README.md` directory is a tracked manifest: **full GPL source**
is in the ignored `third_party/autoas/` checkout, not a second hand-written stack.

## Step 2 — Generate real module configuration and compile

~~~sh
bash tools/build-autoas.sh
~~~

This invokes upstream SCons, which creates actual configuration C files
from its JSON definitions and Controller Area Network (CAN) network database
files. Then GCC compiles and links the AutoAS Electronic Control Unit (ECU).

Read the following input/output pairs directly:

| Input | Output | Why it matters |
| --- | --- | --- |
| `third_party/autoas/app/app/config/Com/Com.json` | `Com/GEN/Com_Cfg.c`, `Com/GEN/CanIf_Cfg.c`, `Com/GEN/PduR_Cfg.c` | Defines Controller Area Network Interface (CanIf) receive filters, mapped Protocol Data Unit Router (PduR) routes, and communication frames. |
| `third_party/autoas/app/app/config/CanTp/CanTp.json` | `CanTp/GEN/CanTp_Cfg.c` | Creates CAN Transport Layer (CanTp) channels for diagnostic addressing and segmentation. |
| `third_party/autoas/app/app/config/Dcm/Dcm.json` | `Dcm/GEN/Dcm_Cfg.c` | Creates Diagnostic Communication Manager (Dcm) services, sessions and Data Identifiers (DIDs). |

Concrete routing: `CanIf_Cfg.c` receives tester messages under identifier
`0x731`, calls `CanTp_RxIndication`, and returns messages on `0x732`
using the configured transmit path. The tester-facing Controller Area Network
simulator runs with no physical transceiver or Linux `vcan` module.

This is a real independent upstream Basic Software implementation. Its
configuration generator is JSON/DBC-driven; **it is not the commercial
ARXML-based full AUTOSAR Runtime Environment (RTE) generation workflow**.

## Step 3 — Run end-to-end diagnostic tests on real modules

~~~sh
python3 tools/test-autoas.py
~~~

The script launches the upstream `CanApp`, not the teaching `lab_ecu`, and
uses the upstream `IsoTpSend` executable linked to `libAsOne.so`.
Test requests and results observed on the Arch host:

| Unified Diagnostic Services (UDS) request | Expected response | Module behavior exercised |
| --- | --- | --- |
| `10 01` | `50 01 00 32 01 F4` | Switch to default session |
| `10 03` | `50 03 00 32 01 F4` | Switch to extended session |
| `22 F1 5B` | `62 F1 5B A5 A5 A5 A5 A5 A5 A5 A5 A5 A5` | Read configured fingerprint (10 bytes); **13-byte** response uses ISO 15765-2 CAN Transport Layer (CanTp) segmentation/reassembly |

The tester's three successful invocations exit with codes **6**, **6** and
**13**, respectively: upstream `IsoTpSend` returns received response **byte
count**, not the usual POSIX success code zero. The runner compares every byte
and checks this behavior, preventing false-negative test reports.

The application is started with an **open standard-input pipe**. This avoids
an upstream interactive-shell bug: launching it with stdin already closed can
flood the log with `shell input buffer overflow!`. The runner starts and stops
only its own process group, and writes a bounded-run report under
`build/stack-evidence/`.

The `simulator_v2` backend uses UDP multicast; it does not emulate electrical
arbitration, acknowledgments or physical Controller Area Network Flexible
Data-Rate (CAN FD) timing. Test on a controlled development network. Setting
`AS_LOCAL_IP=127.0.0.1` did **not** work for this backend on the tested host.

## Step 4 — Inspect actual module code and debug it

- Actual CAN Interface (CanIf) implementation: `third_party/autoas/infras/communication/CanIf/`.
- CAN Transport Layer (CanTp): `third_party/autoas/infras/communication/CanTp/`.
- Protocol Data Unit Router (PduR): `third_party/autoas/infras/communication/PduR/`.
- Diagnostic Communication Manager (Dcm): `third_party/autoas/infras/diagnostic/Dcm/`.
- Native simulated hardware CAN driver: `third_party/autoas/app/platform/simulator/src/Can.cpp`.

The generated `CanIf_Cfg.c` file is the bridge between CAN ID `0x731` and
the transport callback. The generated `Dcm_Cfg.c` file contains service
metadata, and `app/app/src/uds.c` implements `App_ReadFingerPrint`,
which fills 10 bytes with `0xA5` in the host configuration.

Debug session:

~~~sh
cd ~/work/autosar-lab/third_party/autoas
gdb --args build/posix/GCC/CanApp/CanApp
~~~

Set breakpoints in the real modules (`CanTp_RxIndication`,
`Dcm` service handler, `App_ReadFingerPrint`), then run
`python3 tools/test-autoas.py` only when no second Electronic Control Unit (ECU) is already running.
For deeper debugging, launch a separate tester manually with
`IsoTpSend -d CAN.simulator_v2 -v 22F15B`; do not run competing
Electronic Control Unit (ECU) simulators on the same bus unintentionally.

## Step 5 — Generate real Software Component (SWC) Runtime Environment (RTE) interfaces

~~~sh
cd ~/work/autosar-lab
bash tools/generate-autoas-rte.sh
~~~

This invokes the actual AutoAS generator, not handwritten interface stubs.
For its Gauge and Telltale software components, it generates five AUTOSAR
XML (ARXML) files and a `Rte_*.h` interface each, using version-pinned
`autoas/arxml` and `autoas/cfile` libraries. The runner checks that XML
is syntactically parseable and headers contain generated Runtime Environment (RTE) identifiers.

Find the generated port and runnable definitions in
`third_party/autoas/app/app/config/SWC/Gauge/GEN/Gauge.arxml` and
`.../Gauge/GEN/Rte_Gauge.h`. The source template is
`third_party/autoas/app/app/config/SWC/Gauge/rte.py`.

**Current limitations:** the generator explicitly warns that the
Electronic Control Unit State Manager (EcuM) CurrentMode mode-switch interface
is not supported, and the wider `CanIC` application cannot be linked in the
pinned upstream checkout because its generated `VICSkeleton.c` lacks access
to `E2E_Cfg.h`. Do not treat generated ARXML/header files as a working
complete Runtime Environment (RTE) deployment.

## Step 6 — Compare the real OS separately

~~~sh
bash tools/run-trampoline-posix.sh
~~~

Trampoline uses the **OSEK Implementation Language (OIL)** file
`third_party/trampoline/examples/posix/periodic/periodic.oil`,
compiled by its GOIL generator. That generated configuration creates
real tasks/alarms, with nine activations and orderly shutdown observed
on the tested Portable Operating System Interface (POSIX) target.

**The compiled AutoAS `CanApp` is not running on Trampoline.** It uses
AutoAS's Linux Operating System Abstraction Layer (OSAL). Integrating the two
would require deliberate Operating System (OS) Application Programming Interfaces (APIs), Basic Software (BSW) scheduling/configuration and timing work.

## What's still missing for an actual STM32H723 / AT32M416 Electronic Control Unit (ECU)

The Basic Software (BSW) stack/toolchain is now **installed, built and exercised on the host**.
Still not demonstrated: an integrated generated AUTOSAR Runtime Environment,
target-specific Microcontroller Abstraction Layer (MCAL) ports for H723/M416,
a board linker/startup integration, SWD flashing, real CAN FD framing,
physical electrical and fault testing, and AUTOSAR conformance.

See [hardware migration](07-mcu-bringup.md) and
[toolchain overview](../toolchains/README.md).

**Do not deploy this demonstration stack directly into a production vehicle.**
Upstream compilation reported security-related seed functions using
uninitialized local variables, and other development-library warnings. The
positive Unified Diagnostic Services (UDS) test is evidence of the configured diagnostic function, not a
functional-safety or security validation.
