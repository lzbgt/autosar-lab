# External-source exercises (not vendored code)

1. **Trampoline Portable Operating System Interface (POSIX) Operating System (OS):** `docs/06-upstream-stacks.md` and `bash tools/fetch-upstreams.sh trampoline`. Learn OSEK Implementation Language (OIL)/Trampoline configuration generator (GOIL) task, counter, alarm, scheduling and generated Operating System (OS) config.
2. **AutoAS Basic Software (BSW) simulator:** `docs/06-upstream-stacks.md` and `bash tools/fetch-upstreams.sh autoas`. Trace Diagnostic Communication Manager (Dcm)/Protocol Data Unit Router (PduR)/CAN Transport Layer (CanTp)/CAN Interface (CanIf)/Can with real upstream code; native Linux build not guaranteed by project.
3. **Hardware vendor samples:** see `hardware/stm32h723/README.md` and `hardware/at32m416/README.md`. Build and flash separately from vendor-supported BSP before porting teaching modules.

All upstream checkouts live in ignored `third_party/`. The local licensed code under `src/` is kept self-contained and readable.
