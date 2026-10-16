# Actual AUTOSAR-related toolchains installed in this project

## Host requirements

Validated on Arch Linux x86-64 using GCC, GNU Arm embedded GCC, CMake, Git,
Python 3.14.8, SCons 4.11.1 and the Trampoline GOIL compiler. The working
AutoAS configuration generator dependencies are **version-pinned** in
`toolchains/autoas-requirements.txt`.

Install or refresh the local environment:

~~~sh
cd ~/work/autosar-lab
bash tools/setup-autoas.sh
bash tools/build-autoas.sh
python3 tools/test-autoas.py
~~~

The setup script checks out **real upstream AutoAS source** at a pinned
commit, then installs Python SCons, pycrc, Jinja2 and other required packages
inside `.venv-autoas/`. The same requirements also pin `lxml==6.1.3`, used
for parsing and generating AUTOSAR XML (ARXML) in the Runtime Environment (RTE)
exercise. This environment is ignored by Git and does not replace
the system's Python packages. If the `uv` package manager is unavailable,
standard Python virtual environment and pip provide a fallback.

The build script executes the actual upstream targets in dependency order:

~~~sh
cd third_party/autoas
../../.venv-autoas/bin/scons --app=CanApp -j4
../../.venv-autoas/bin/scons --lib=AsOne -j4
../../.venv-autoas/bin/scons --app=IsoTpSend -j4
~~~

**Config input → generated source → compiled Basic Software (BSW):**

| Input | Generated C output in `third_party/autoas/app/app/config/` |
| --- | --- |
| `Com/Com.json` and `Com/CAN0.dbc` | `Com/GEN/Com_Cfg.c` and `Com/GEN/CanIf_Cfg.c`, `Com/GEN/PduR_Cfg.c` |
| `CanTp/CanTp.json` | `CanTp/GEN/CanTp_Cfg.c` |
| `Dcm/Dcm.json` | `Dcm/GEN/Dcm_Cfg.c` and `Dcm/GEN/Dem_Cfg.c` |

Open these real files to follow configuration into dispatch callbacks.
For instance, `CanIf_Cfg.c` maps tester request identifier `0x731` to
`CanTp_RxIndication`; the configured transmit identifier `0x732` invokes
the corresponding confirmation path. `Dcm.json` defines diagnostic session
`0x01` and `0x03`, and ReadDataByIdentifier (service `0x22`) identifier
`0xF15B`, returning ten `A5` bytes after the response header.

This is AutoAS's own JSON/database-driven generator workflow, **not**
an AUTOSAR-standard ARXML-to-Runtime Environment (RTE) commercial toolchain.
The source repository also includes an `Rte.py` generator, but the compiled
host `CanApp` demo does not demonstrate a fully generated, integrated
AUTOSAR Runtime Environment.

## Real AutoAS Software Component (SWC) generator

~~~sh
bash tools/generate-autoas-rte.sh
~~~

The command fetches and pins upstream `autoas/arxml` at commit
`694b75ffe4c1cd7c4c65f23af036f37dd2eabfdd` and `autoas/cfile` at
`7ed5d2584e4c711d2fb219dfdf228f100f609186`, checks real input
communication configuration, then invokes the published Gauge/Telltale
software-component examples. It verifies generated `Rte_Gauge.h`,
`Rte_Telltale.h`, and ten parseable AUTOSAR XML files in ignored
`third_party/autoas/app/app/config/SWC/{Gauge,Telltale}/GEN/`.

This is genuine upstream code generation, but **not an integrated runnable
Runtime Environment (RTE)**: the mode-switch interface generator emits an
unsupported warning and the separate `CanIC` build currently has a missing
`E2E_Cfg.h` include dependency. These are known remaining integration gaps.

## Real operating-system configuration generator

Trampoline's GOIL translates OSEK Implementation Language (OIL) to generated
task/alarms/configuration and build files:

~~~sh
bash tools/run-trampoline-posix.sh
~~~

Source configuration:
`third_party/trampoline/examples/posix/periodic/periodic.oil`.
Generated outputs live inside the ignored upstream working directory.
The tested example produced nine task activations and shutdown.

## Important portability and licensing limits

- **Not yet deployable to the user's STM32H723 and AT32M416:** full firmware
  requires startup/linker scripts, target-specific Microcontroller
  Abstraction Layer (MCAL) drivers, task integration and board pin configuration.
- **Not a turnkey complete Classic ECU:** the AutoAS host executable and
  Trampoline operating system are two independently compiled examples.
- AutoAS license is GPL version 3 or commercial; Trampoline is GPL version 2.
  The repo's teaching C files remain MIT licensed and are separate.
- AutoAS SCons may fetch upstream transitive sources into
  `third_party/download/` (Lua, JSON library, CRC code generator, file-system
  libraries). Some downloads follow the upstream build's tags/branches rather
  than a complete independently locked manifest; the tested commit and Python
  dependencies are pinned, but fresh network builds are not fully hermetic.
- Compiler warnings in upstream security-seed functions include uninitialized
  local values. Do not reuse those routines for authentication or deploy
  this upstream snapshot in a safety-critical product without fixing and
  validating the defects.

Source references:
[AutoAS](https://github.com/autoas/as),
[Trampoline](https://github.com/TrampolineRTOS/trampoline),
[Classic Platform architecture](https://www.autosar.org/standards/classic-platform/).
