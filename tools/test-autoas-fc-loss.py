#!/usr/bin/env python3
"""Real M_CAN frame suppression -> real AutoAS CanTp N_Bs timeout.

Fault is injected *after* the emulated M_CAN controller received a real
Flow Control (FC) frame: the server driver's FIFO callback deliberately
discards it before passing the frame to the upstream CAN Interface.
This is not a simulation of voltage-level CAN loss or bus arbitration.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/emu/h743-autoas-dcm"
BASE = ROOT / "hardware/simulator/h743-autoas-dcm/two-node.resc"
RENODE = ROOT / "third_party/renode-1.17.0/renode"


def parse(path: Path) -> tuple[str, list[int]]:
    body = path.read_text(errors="replace").replace("\r", "")
    return body, [
        int(x, 16)
        for x in re.findall(r"(?m)^\s*0x([0-9A-Fa-f]{8})\s*$", body)
    ]


def validate(values: list[int]) -> None:
    if len(values) != 28:
        raise ValueError(f"Expected 28 Cortex-M7 SRAM readings, got {len(values)}")
    tester = values[:10]
    server = values[10:23]
    dropped, started, ended, elapsed, idle = values[23:]
    if server[0] != 0xFA17FC00:
        raise ValueError(f"Real server timeout not observed: {server[0]:#x}")
    if tester[0] == 0xCAFE220D or tester[1] != 0:
        raise ValueError("Tester falsely accepted incomplete 13-byte diagnostic reply")
    if server[1:5] != [1, 1, 0, 0]:
        raise ValueError(f"Unexpected real Dcm/CanTp/DET state: {server[1:5]}")
    if server[5:10] != [1, 0, 0, 1, 0]:
        raise ValueError(f"Server must receive SF and FC, never CF: {server[5:10]}")
    if tester[6:9] != [1, 0, 0]:
        raise ValueError(f"Tester must receive FF but no CF or FC: {tester[6:9]}")
    if dropped != 1 or idle != 1:
        raise ValueError(f"FC drop or Dcm buffer-idle reset absent: {dropped=}, {idle=}")
    if not (0 < started < ended and 900 <= elapsed <= 1100):
        raise ValueError(
            f"CanTp N_Bs ~1000ms timeout not observed: "
            f"{started=}, {ended=}, {elapsed=}"
        )
    irq, fast, main = server[10:13]
    if not (1320 <= irq <= 1380 and irq - 5 <= fast <= irq and main == fast // 10):
        raise ValueError(f"Wrong SysTick 1ms/10ms schedule: {irq=}, {fast=}, {main=}")


def validate_recovery(values: list[int]) -> None:
    """Same server survives a real timeout, then serves a fresh UDS request."""
    if len(values) != 39:
        raise ValueError(f"Expected 39 independent SRAM reads, got {len(values)}")
    first, server = values[:10], values[10:23]
    later, fault = values[23:33], values[33:]
    if first[0] == 0xCAFE220D or first[6:8] != [1, 0]:
        raise ValueError("First diagnostic request improperly completed despite missing FC")
    if server[0] != 0xFA17C003 or server[1:5] != [1, 2, 0, 0]:
        raise ValueError(f"Wrong recovered-server status or confirmations: {server[:5]}")
    if server[5:10] != [2, 0, 0, 1, 1]:
        raise ValueError(f"Server should receive two SFs and one suppressed FC: {server[5:10]}")
    if later[:5] != [0xCAFE1003, 1, 1, 0, 1]:
        raise ValueError(f"New tester did not verify full positive session reply: {later[:5]}")
    if later[5:10] != [1, 0, 0, 0, 0]:
        raise ValueError(f"New tester must receive one single frame: {later[5:10]}")
    dropped, start, end, elapsed, idle, recovered = fault
    if dropped != 1 or idle != 1 or recovered != 1:
        raise ValueError(f"Missing real M_CAN loss, Dcm reset or later recovery: {fault}")
    if not (0 < start < end and elapsed == end-start and 900 <= elapsed <= 1100):
        raise ValueError(f"Real CanTp N_Bs timeout not established: {fault}")
    irq, fast, main = server[10:13]
    if not (1450 <= irq <= 1550 and irq-5 <= fast <= irq and main==fast//10):
        raise ValueError(f"Wrong Cortex-M7 1/10ms timer: {irq=}, {fast=}, {main=}")


def recovery_self_test() -> None:
    sample = (
        [0xBAD20001, 1, 2, 0, 1, 2, 1, 0, 0, 0]
        + [0xFA17C003, 1, 2, 0, 0, 2, 0, 0, 1, 1, 1500, 1499, 149]
        + [0xCAFE1003, 1, 1, 0, 1, 1, 0, 0, 0, 0]
        + [1, 10, 1010, 1000, 1, 1]
    )
    validate_recovery(sample)
    for name, offset, wrong in (
        ("premature success", 0, 0xCAFE220D),
        ("server never recovered", 10, 0xFA17FC00),
        ("lost raw FC count", 18, 0),
        ("lost session callback", 19, 0),
        ("late tester wrong reply", 23, 0),
        ("premature N_Bs expiry", 36, 150),
        ("missing response recovery", 38, 0),
        ("wrong SysTick cadence", 20, 159),
    ):
        bad = sample.copy()
        bad[offset] = wrong
        try:
            validate_recovery(bad)
        except ValueError:
            pass
        else:
            raise AssertionError(f"Recovery checker accepted {name}")
    print("PASS: eight recovery evidence corruption cases rejected")


def self_test() -> None:
    tester = [0x50000003, 0, 2, 0, 0, 0, 1, 0, 0, 0]
    server = [0xFA17FC00, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1350, 1349, 134]
    extra = [1, 15, 1015, 1000, 1]
    valid = tester + server + extra
    validate(valid)
    cases = (
        ("missing FC drop", 23, 0),
        ("missing timeout start", 24, 0),
        ("early timeout", 26, 500),
        ("false Dcm success", 10, 0xCAFE22DC),
        ("false tester success", 0, 0xCAFE220D),
        ("wrong hardware clock", 20, 159),
    )
    for label, index, bad_value in cases:
        broken = valid.copy()
        broken[index] = bad_value
        try:
            validate(broken)
        except ValueError:
            pass
        else:
            raise ValueError(f"Validator accepted corrupted evidence: {label}")
    print("PASS: six negative tests reject false timeout/recovery evidence")


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        self_test()
        recovery_self_test()
        return 0
    self_test()
    recovery_self_test()
    src = BASE.read_text()
    server_image = "@build/emu/h743-autoas-dcm/server.elf"
    assert src.count(server_image) == 1
    src = src.replace(
        server_image, "@build/emu/h743-autoas-dcm/server-fc-loss.elf"
    )
    duration = 'emulation RunFor "1.2"'
    assert src.count(duration) == 1
    src = src.replace(duration, 'emulation RunFor "1.35"')
    src += "\n# Fault injection milestones on server CPU.\n"
    for offset in range(0x4C, 0x60, 4):
        src += f"sysbus ReadDoubleWord 0x240000{offset:02X}\n"
    OUT.mkdir(parents=True, exist_ok=True)
    scene = OUT / "fc-loss.resc"
    scene.write_text(src)
    log = OUT / "fc-loss.log"
    with log.open("w") as output:
        subprocess.run(
            [
                str(RENODE), "--disable-gui", "-P", "-1",
                "--console", "--plain",
                "-e", "include @build/emu/h743-autoas-dcm/fc-loss.resc; quit",
            ],
            cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
            check=True, timeout=110,
        )
    body, values = parse(log)
    if "[ERROR]" in body or "[FATAL]" in body:
        raise ValueError("Renode logged an emulator error")
    print("Tester:", [hex(x) for x in values[:10]])
    print("Server:", [hex(x) for x in values[10:23]])
    print("FC dropped; start ms; end ms; N_Bs elapsed ms; Dcm idle:", values[23:])
    validate(values)
    print("PASS: driver dropped actual received FC before CanIf, not before M_CAN FIFO")
    print("PASS: AutoAS CanTp N_Bs timeout aborted transport after ~1000 ms")
    print("PASS: Dcm transmit buffer returned idle; incomplete UDS reply was rejected")

    # Keep the FIRST server alive; connect a SECOND real Cortex-M7 tester
    # after its 1000ms N_Bs timeout. The new tester sends UDS 10 03.
    recovery_scene = BASE.read_text().replace(
        server_image, "@build/emu/h743-autoas-dcm/server-fc-loss.elf"
    )
    assert recovery_scene.count(duration) == 1
    recovery_scene = recovery_scene.replace(
        duration, """emulation RunFor "1.15"
mach create "recovery"
machine LoadPlatformDescription @platforms/cpus/stm32h743.repl
sysbus LoadELF @build/emu/h743-autoas-dcm/tester-session.elf
connector Connect fdcan1 canHub
emulation RunFor "0.35" """
    )
    recovery_scene += '\nmach set "recovery"\n'
    for offset in (0, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C):
        recovery_scene += f"sysbus ReadDoubleWord 0x240000{offset:02X}\n"
    recovery_scene += 'mach set "server"\n'
    for offset in range(0x4C, 0x64, 4):
        recovery_scene += f"sysbus ReadDoubleWord 0x240000{offset:02X}\n"
    second = OUT / "fc-recovery.resc"
    second.write_text(recovery_scene)
    second_log = OUT / "fc-recovery.log"
    with second_log.open("w") as output:
        subprocess.run(
            [
                str(RENODE), "--disable-gui", "-P", "-1", "--console", "--plain",
                "-e", "include @build/emu/h743-autoas-dcm/fc-recovery.resc; quit",
            ],
            cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
            check=True, timeout=110,
        )
    second_body, recovered = parse(second_log)
    if "[ERROR]" in second_body or "[FATAL]" in second_body:
        raise ValueError("Renode logged an error in sequential recovery")
    validate_recovery(recovered)
    print("PASS: same real Dcm MCU recovered after N_Bs timeout without reboot")
    print("PASS: later Cortex-M7 tester validated full positive UDS 10 03 reply")
    print(f"Renode fault evidence: {log} and {second_log}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (
        OSError, ValueError, RuntimeError, AssertionError,
        subprocess.CalledProcessError, subprocess.TimeoutExpired,
    ) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
