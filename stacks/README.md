# Real automotive software stacks in this lab

**This directory is the manifest, not a toy reimplementation.** The actual
source is installed in the ignored `third_party/` directory by version-pinned
checkout scripts, so the project Git repository stays small.

## 1. AutoAS — real AUTOSAR-oriented Basic Software (BSW)

- **Source checkout:** `third_party/autoas/`
- **Pinned commit:** `7bf0210644a932ae45e6cabbe8388729808af7fd`
- **License:** GPL version 3 or a separately negotiated commercial license.
- **Build system:** SCons, using Python configuration generators.
- **Compiled Electronic Control Unit (ECU):** `third_party/autoas/build/posix/GCC/CanApp/CanApp`
- **Native transport/tester library:** `third_party/autoas/build/posix/GCC/prebuilt/libAsOne.so`
- **Unified Diagnostic Services (UDS) tester:** `third_party/autoas/build/posix/GCC/IsoTpSend/IsoTpSend`
- **Target:** Linux host with AutoAS `simulator_v2` network backend and its Operating System Abstraction Layer (OSAL).

This is actual upstream software, not the handwritten `src/core/` example. The
following implementation directories supply real modules:

| Module name and purpose | Source location under `third_party/autoas/` |
| --- | --- |
| Communication module (Com): signal packaging | `infras/communication/Com/` |
| Protocol Data Unit Router (PduR): route configured messages | `infras/communication/PduR/` |
| Controller Area Network Interface (CanIf): map upper-layer messages to controller requests | `infras/communication/CanIf/` |
| CAN Transport Layer (CanTp): ISO 15765-2 segmentation and flow control | `infras/communication/CanTp/` |
| Diagnostic Communication Manager (Dcm): interpret diagnostic services | `infras/diagnostic/Dcm/` |
| Diagnostic Event Manager (Dem): manage fault events and statuses | `infras/diagnostic/Dem/` |

The normal full module paths and generated configuration files should be
inspected in the checkout itself; source module availability does not imply every
module is active in every application profile.

AutoAS does **not** directly support the STM32H723 or AT32M416 MCUs in this
example. The host's simulated driver is not a physical Microcontroller
Abstraction Layer (MCAL) port for either board.

## 2. Software Component (SWC) / Runtime Environment (RTE) generation

The AutoAS checkout includes a real generator under `tools/generator/Rte.py`.
The pinned Gauge/Telltale examples define software-component ports, data types,
runnable timing, and interfaces via Python templates and use the AutoAS XML generator.
Run:

~~~sh
cd ~/work/autosar-lab
bash tools/generate-autoas-rte.sh
~~~

Results under `third_party/autoas/app/app/config/SWC/{Gauge,Telltale}/GEN/`:
`Rte_Gauge.h`, `Rte_Telltale.h`, and five parseable AUTOSAR XML (ARXML)
descriptions for each component. This **does not** produce a complete generated
and linked Runtime Environment (RTE); the upstream generator warns that the
Electronic Control Unit State Manager (EcuM) CurrentMode mode-switch interface
is not supported. Full `CanIC` application linking currently fails on a
separate generated `E2E_Cfg.h` include path.

## 3. Trampoline — real operating-system layer, separate example

- **Source:** `third_party/trampoline/`
- **Pinned commit:** `ff287023252bf73273067f00d2dbc8bbc9a20401`
- **License:** GPL version 2.
- **Configuration compiler:** `third_party/trampoline/goil/makefile-unix/goil`
- **OS source:** `third_party/trampoline/os/`
- **Task/alarm configuration:** `third_party/trampoline/examples/posix/periodic/periodic.oil`
- **Processor emulator:** `third_party/trampoline/viper/`

Trampoline's OSEK/AUTOSAR-aligned Operating System (OS) is successfully built
and executed with its own Portable Operating System Interface (POSIX) backend.
**It is not linked into AutoAS's current Linux `CanApp` executable.**

## 4. One verified workflow

~~~sh
cd ~/work/autosar-lab
bash tools/run-real-autosar.sh
~~~

For a fast inspection of installed binaries and generators:

~~~sh
bash tools/stack-status.sh
~~~

Read [the genuine stack walkthrough](../docs/11-real-autosar-stack.md) and
[toolchain setup](../toolchains/README.md). All commands require only free
software. No commercial AUTOSAR generator/license is included.
