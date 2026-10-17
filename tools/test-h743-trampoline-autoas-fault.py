#!/usr/bin/env python3
"""Test real AutoAS N_Bs fault and recovery on the same Trampoline-scheduled ECU.

Two emulated Cortex-M7 testers are independent of the genuine Trampoline
kernel + AutoAS BSW server. The M_CAN driver receives an actual FC frame
but deliberately suppresses delivery to CanIf in this test-only firmware.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/emu/h743-trampoline-autoas"
RENODE = ROOT / "third_party/renode-1.17.0/renode"

NORMAL = "@build/emu/h743-trampoline-autoas/trampoline_autoas.elf"
FAULT = "@build/emu/h743-trampoline-autoas/trampoline_autoas-fc-loss.elf"


def load(path: str, name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to import existing validator: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def os_schedule(words: list[int], low: int, high: int) -> None:
    # Server words 16-18: AutoAS 1-ms, fast, 10-ms; words 25-26: OSEK task
    # counter and tenth-task indicator. They are genuinely independent.
    server = words[10:23]
    os_fast, os_main = words[23:25]
    bsw_irq, bsw_fast, bsw_main = server[10:13]
    if not (low <= os_fast <= high):
        raise ValueError(f"Unexpected Trampoline task count: {os_fast}")
    if not (0 <= os_fast - bsw_fast <= 2 and abs(os_fast - bsw_irq) <= 2):
        raise ValueError(
            f"OS and BSW task counts diverged: {os_fast}, {bsw_fast}, {bsw_irq}"
        )
    if os_main != bsw_main or os_main != os_fast // 10:
        raise ValueError(
            f"Trampoline 10-ms cadence differs from Dcm: {os_main}, {bsw_main}"
        )


def validate_timeout(words: list[int], original) -> None:
    if len(words) != 30:
        raise ValueError(f"Expected 30 real CPU SRAM samples, got {len(words)}")
    original.validate(words[:23] + words[25:])
    os_schedule(words, 1330, 1400)


def validate_recovery(words: list[int], original) -> None:
    if len(words) != 41:
        raise ValueError(f"Expected 41 real CPU SRAM samples, got {len(words)}")
    original.validate_recovery(words[:23] + words[25:])
    os_schedule(words, 1480, 1550)


def self_test(original) -> None:
    original.self_test()
    original.recovery_self_test()
    timeout = (
        [0x50000003, 0, 2, 0, 1, 0, 1, 0, 0, 0]
        + [0xFA17FC00, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1369, 1369, 136]
        + [1369, 136] + [1, 21, 1020, 999, 1]
    )
    recovery = (
        [0xBAD20001, 1, 2, 0, 1, 2, 1, 0, 0, 0]
        + [0xFA17C003, 1, 2, 0, 0, 2, 0, 0, 1, 1, 1519, 1519, 151]
        + [1519, 151]
        + [0xCAFE1003, 1, 1, 0, 1, 1, 0, 0, 0, 0]
        + [1, 21, 1020, 999, 1, 1]
    )
    validate_timeout(timeout, original)
    validate_recovery(recovery, original)
    cases = [
        ("false incomplete-response acceptance", timeout, validate_timeout, 0, 0xCAFE220D),
        ("lost Flow Control observation", timeout, validate_timeout, 25, 0),
        ("incorrect N_Bs timeout", timeout, validate_timeout, 28, 400),
        ("missing OSEK task executions", timeout, validate_timeout, 23, 0),
        ("10-ms OSEK/Dcm mismatch", timeout, validate_timeout, 24, 0),
        ("unexpected development error", timeout, validate_timeout, 13, 1),
        ("no second UDS response", recovery, validate_recovery, 25, 0),
        ("no real Dcm recovery marker", recovery, validate_recovery, 10, 0xFA17FC00),
        ("missing second transmit confirmation", recovery, validate_recovery, 12, 1),
        ("wrong post-recovery clock", recovery, validate_recovery, 23, 159),
    ]
    for name, good, validator, index, bad_value in cases:
        broken = good.copy()
        broken[index] = bad_value
        try:
            validator(broken, original)
        except ValueError:
            continue
        raise ValueError(f"Accepted corrupted OSEK fault/recovery evidence: {name}")
    print("PASS: ten additional OSEK/AutoAS timeout/recovery corruptions rejected")


def read_words(machine: str, offsets: list[int]) -> str:
    return f'mach set "{machine}"\n' + "".join(
        f"sysbus ReadDoubleWord 0x240000{offset:02X}\n" for offset in offsets
    )


def build_scenarios(regular) -> tuple[Path, Path]:
    initial = regular.generate_scenario("did", regular.CASES["did"])
    template = initial.read_text()
    if template.count(NORMAL) != 1:
        raise ValueError("Expected exactly one original Trampoline server")
    base = template.replace(NORMAL, FAULT)
    if base.count('emulation RunFor "1.2"') != 1:
        raise ValueError("Unexpected emulation duration")

    # First experiment: no later tester; the Dcm response must abort.
    failure = base.replace('emulation RunFor "1.2"', 'emulation RunFor "1.35"')
    failure += "\n# Fault milestones in real OSEK server SRAM.\n"
    failure += read_words("server", list(range(0x4C, 0x60, 4)))
    failure_path = OUT / "osek-fc-loss.resc"
    failure_path.write_text(failure)

    # Second experiment: original server stays alive and resumes handling
    # a second tester after the 1000-ms N_Bs timeout. Do not reboot it.
    if base.count('mach set "tester"') != 1:
        raise ValueError("Unexpected source monitor readout section")
    prefix = base[:base.index('mach set "tester"')]
    prefix = prefix.replace('emulation RunFor "1.2"', 'emulation RunFor "1.15"')
    prefix += """mach create "recovery"
machine LoadPlatformDescription @platforms/cpus/stm32h743.repl
sysbus LoadELF @build/emu/h743-autoas-dcm/tester-session.elf
connector Connect fdcan1 canHub
emulation RunFor "0.35"

"""
    peer = [0, *range(0x1C, 0x40, 4)]
    server = [0, *range(0x1C, 0x4C, 4)]
    prefix += read_words("tester", peer)
    prefix += read_words("server", server + [0x64, 0x68])
    prefix += read_words("recovery", peer)
    prefix += read_words("server", list(range(0x4C, 0x64, 4)))
    recovery_path = OUT / "osek-fc-recovery.resc"
    recovery_path.write_text(prefix)
    return failure_path, recovery_path


def run_scenario(label: str, scene: Path, original, validator) -> None:
    log = OUT / f"osek-{label}.log"
    with log.open("w") as output:
        subprocess.run(
            [
                str(RENODE), "--disable-gui", "-P", "-1", "--console", "--plain",
                "-e", f"include @build/emu/h743-trampoline-autoas/{scene.name}; quit",
            ],
            cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True, timeout=120,
        )
    text, words = original.parse(log)
    if "[ERROR]" in text or "[FATAL]" in text:
        raise ValueError(f"Emulator failed in {label}: {log}")
    validator(words, original)
    print(
        f"PASS real OSEK/AutoAS {label}: "
        f"OS 1ms={words[23]}, 10ms={words[24]}, "
        f"lost-FC={words[25 if label == 'fc-loss' else 35]}"
    )
    print(f"  verified hardware evidence: {log}")


def main() -> int:
    baremetal = load("test-autoas-fc-loss.py", "baremetal_fc_loss")
    self_test(baremetal)
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        return 0
    if len(sys.argv) != 1:
        raise ValueError("Usage: test-h743-trampoline-autoas-fault.py [--self-test]")
    integrated = load("test-h743-trampoline-autoas.py", "trampoline_uds")
    OUT.mkdir(parents=True, exist_ok=True)
    first, second = build_scenarios(integrated)
    run_scenario("fc-loss", first, baremetal, validate_timeout)
    run_scenario("fc-recovery", second, baremetal, validate_recovery)
    print("PASS: real Trampoline+AutoAS timeout and post-timeout recovery on same MCU")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as exc:
        print(f"FAIL integrated OS fault/recovery: {exc}", file=sys.stderr)
        sys.exit(1)
