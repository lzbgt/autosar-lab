# 02 — Components, ports, Runtime Environment (RTE) and AUTOSAR Operating System (OS)

## What a Software Component (SWC) contains

- **Ports:** Required (R-Port) or provided (P-Port), typed by sender-receiver, client-server, mode-switch, parameter or other interfaces.
- **Runnables:** Functions mapped to triggering events, e.g. timing event every 10 ms, data-received event, operation invoked.
- **Internal behavior:** State, inter-runnable variables, exclusive areas, calibration/constant parameters and runnable-to-task mapping.
- **Composition:** Software Components (SWCs) connected by assembly connectors, then mapped to Electronic Control Units (ECUs).

A readable *conceptual* example:

~~~c
/* Pseudocode only; actual names/types come from RTE generation. */
void SpeedSensor_Runnable_10ms(void) {
    uint16_t speed_cm_s = read_sensor();
    (void)Rte_Write_P_Speed_Value(speed_cm_s);
}
void SpeedControl_Runnable_10ms(void) {
    uint16_t speed_cm_s;
    if (Rte_Read_R_Speed_Value(&speed_cm_s) == RTE_E_OK) {
        controller_update(speed_cm_s);
    }
}
~~~

The generated interfaces handle the chosen software-component mapping and configured communication policy. C function calls alone do not represent a Runtime Environment (RTE).

## One value, two different mechanisms: scheduling and data movement

A **software runnable** is a function, not automatically an operating-system task. An **Operating System (OS)** uses an alarm or schedule table to release a configured task, which may call one or several runnables. The **Runtime Environment (RTE)** moves values between Software Component (SWC) ports and configured services; it does not decide CPU preemption or interrupt priority.

For the teaching implementation:

1. Linux calls `lab_tick` repeatedly; it emulates a 10 ms runnable release (not a certified real-time schedule).
2. `lab_application_run_10ms` increments synthetic speed by 5 cm/s.
3. The 100 ms dispatcher calls `lab_rte_com_publish_speed`, which directly packs bytes in this example, even though a real generated Runtime Environment (RTE) would separate its port interface from the Communication module (Com).
4. At the first 100 ms boundary, speed is `50 cm/s`; the CAN frame is `0x201#3200000100000000` (ID followed by eight payload bytes).
5. `src/tests/test_core.c` checks the 100 ms result; `tools/smoke.py` checks the live transport and diagnostic response.

Try changing task timing without changing signal packing, then signal packing without changing timing. That separates **who schedules computation** (Operating System) from **who moves typed data** (Runtime Environment and communication modules).

The pinned Trampoline example is a genuine OSEK/AUTOSAR-OS-aligned implementation: `periodic.oil` defines a task and counter-based alarm, its configuration generator produces source, and ViPER provides a POSIX simulation environment. Its alarm counts use a configured counter; do not assume `100` counter ticks means `100 ms` unless the counter's resolution is known.


## Scheduler anatomy

In Classic, the Operating System (OS) manages tasks, interrupts, timing services, resources and schedule tables. Depending on Operating System (OS) configuration, tasks can be basic or extended, preemptive or non-preemptive, and event-driven. Important terms:

| Concept | Meaning |
| --- | --- |
| Task | Scheduled execution context; may contain one or more runnables |
| Alarm / Counter | Counter expiration may activate a task or set an event |
| Schedule table | Time-based sequence of expiry points |
| ISR category | Interrupt interactions and Operating System (OS) Application Programming Interface (API) restrictions depend on category |
| Resource / exclusive area | Serializes access; priority ceiling/protection prevents races |
| Activation limit | Bound on pending task activations |
| Stack / memory protection | Stack sizes, protection and execution isolation vary by Operating System (OS) class |

Simple time line:

~~~text
t(ms)      0   10   20   30   ...  90  100 110
10ms task      A    A    A              A    A
100ms COM task                              C
~~~

Real Operating System (OS) scheduling introduces priorities, blocking, preemption, interrupts, jitter and Worst-Case Execution Time (WCET). A Linux timer callback is not equivalent to guaranteed Microcontroller Unit (MCU) task activation.

### Response-time sanity check

If a periodic task must finish before its next period, a necessary (not sufficient) condition is its worst-case execution time C less than period T. For n tasks on a single core, total utilization is:

~~~text
U = sum_i(C_i / T_i)
~~~

For example, 10 ms task with C=1 ms and 100 ms task with C=2 ms:

~~~text
U = 1/10 + 2/100 = 0.12 = 12%
~~~

This is *not* proof of schedulability: interrupts, non-preemptive blocking, priorities, network timing, Operating System (OS) overhead and bursts matter.

## Inspect the runnable in this repository

- `src/core/lab_ecu.c`: `lab_application_run_10ms` updates synthetic speed 5 cm/s per activation; `lab_tick` is a deterministic **host dispatcher** with bounded catch-up.
- `src/core/lab_com.c`: `lab_rte_com_publish_speed` packs a speed signal and passes it to an educational Protocol Data Unit Router (PduR)/CAN Interface (CanIf) handoff.
- `src/host/main.c`: uses a Linux monotonic clock and `poll` to call `lab_tick`.

**Important:** Our dispatcher has no task priorities, semaphores, ISR contexts, safety scheduling, generated Runtime Environment (RTE) or AUTOSAR Operating System (OS) semantics. Those topics belong to the Trampoline Portable Operating System Interface (POSIX) lab in `06-upstream-stacks.md`.

## Hands-on experiment

Change the increment in `lab_application_run_10ms` from 5 to 7 cm/s and predict byte 0,1 of the first speed frame at t=100ms:

~~~text
10 executions × 7 cm/s = 70 cm/s
Bytes 0,1 (little endian): 46 00 hex
~~~

Build, run and observe via `tools/smoke.py` (it checks existence/type of telemetry, not this exact value); use `--verbose` for full Transmit (TX) frame. Restore the value and rerun CTest when done.

## What to learn next with a genuine AUTOSAR-aligned Operating System (OS)

Use Trampoline Portable Operating System Interface (POSIX) to study `TASK`, `ActivateTask`, `SetRelAlarm`, `TerminateTask`, generated Operating System (OS) config and OSEK Implementation Language (OIL). Compare the results with the Linux dispatcher; do not confuse OSEK Implementation Language (OIL) with full AUTOSAR XML (ARXML)/Electronic Control Unit Configuration (ECUC). See `06-upstream-stacks.md`.

Official entry: https://www.autosar.org/standards/classic-platform/
