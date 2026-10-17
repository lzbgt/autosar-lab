#!/usr/bin/env python3
"""One real MCU ELF: upstream Trampoline OSEK task schedules upstream AutoAS.

This runner verifies three UDS services with an independent upstream
AutoAS CanTp tester. The Trampoline server must start before the tester:
the OSEK startup configures M_CAN from the generated 1-ms task.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/emu/h743-trampoline-autoas"
TEMPLATE = ROOT / "hardware/simulator/h743-autoas-dcm/two-node.resc"
RENODE = ROOT / "third_party/renode-1.17.0/renode"
CHECK = ROOT / "tools/check-autoas-dcm.py"
SERVER = "@build/emu/h743-trampoline-autoas/trampoline_autoas.elf"
BASE_SERVER = "@build/emu/h743-autoas-dcm/server.elf"
BASE_TESTER = "@build/emu/h743-autoas-tp/tester.elf"
CASES = {
    "did": BASE_TESTER,
    "session": "@build/emu/h743-autoas-dcm/tester-session.elf",
    "reject": "@build/emu/h743-autoas-dcm/tester-reject.elf",
}


def read_status(log: Path) -> tuple[str, list[int]]:
    data = log.read_text(errors="replace").replace("\r", "")
    values = [
        int(value, 16)
        for value in re.findall(r"(?m)^\s*0x([0-9A-Fa-f]{8})\s*$", data)
    ]
    return data, values


def validate_integrated(kind: str, words: list[int], validate_dcm) -> None:
    if len(words) != 25:
        raise ValueError(f"Expected 25 SRAM words, got {len(words)}")
    # Reuse exact, independently tested response/physical frame assertions
    # from standalone real AutoAS rather than weakening acceptance.
    validate_dcm(kind, words[:23])
    os_one_ms, os_ten_ms = words[23:]
    bsw_one_ms, bsw_ten_ms = words[21:23]
    if not (1180 <= os_one_ms <= 1250):
        raise ValueError(f"Genuine OSEK 1-ms task frequency wrong: {os_one_ms}")
    if not (0 <= os_one_ms - bsw_one_ms <= 3):
        raise ValueError(
            f"Trampoline and AutoAS 1-ms execution counts disagree: "
            f"OS={os_one_ms}, CanTp={bsw_one_ms}"
        )
    if os_ten_ms != bsw_ten_ms or os_ten_ms != os_one_ms // 10:
        raise ValueError(
            f"10-ms Dcm task scheduling mismatch: "
            f"OS={os_ten_ms}, Dcm={bsw_ten_ms}"
        )


def generate_scenario(kind: str, tester: str) -> Path:
    source = TEMPLATE.read_text()
    for marker in (BASE_SERVER, BASE_TESTER, 'mach create "server"',
                   'mach create "tester"', 'emulation RunFor "1.2"'):
        if source.count(marker) != 1:
            raise RuntimeError(f"Unexpected source template marker: {marker}")
    # The actual OSEK process must initialize its CAN controller first.
    # A too-early tester frame can otherwise be lost; a 20-ms lead is
    # explicit and testable, not a protocol workaround in the firmware.
    s = source.index('mach create "tester"')
    m = source.index('mach create "server"')
    e = source.index('emulation SetGlobalQuantum', m)
    if not (s < m < e):
        raise RuntimeError("Unexpected original Renode scenario structure")
    peer = source[s:m].replace(BASE_TESTER, tester)
    server = source[m:e].replace(BASE_SERVER, SERVER)
    reads = source[source.index('mach set "tester"', e):]
    result = (
        'emulation CreateCANHub "canHub"\n'
        'emulation SetGlobalQuantum "0.000025"\n'
        + server
        + 'emulation RunFor "0.02"\n'
        + peer
        + 'emulation RunFor "1.2"\n\n'
        + reads
        + '\n# Genuine Trampoline task invocation statistics.\n'
        + 'sysbus ReadDoubleWord 0x24000064\n'  # OS task count
        + 'sysbus ReadDoubleWord 0x24000068\n'  # 10ms count
    )
    destination = OUT / f"integration-{kind}.resc"
    destination.write_text(result)
    return destination


def self_test(validate_dcm) -> None:
    for name in CASES:
        # Real UDS values sourced directly from the original checker.
        full = validate_dcm.EXPECTED[name]
        data = list(full[0] + full[1]) + [1219, 1219, 121, 1219, 121]
        validate_integrated(name, data, validate_dcm.validate)
        for label, i, bad_value in (
            ("bad UDS response", 0, 0),
            ("missing OS task", 23, 0),
            ("wrong OS cadence", 24, 5),
            ("BSW OS drift", 21, 800),
            ("unexpected Dcm error", 13, 1),
        ):
            damaged = data.copy()
            damaged[i] = bad_value
            try:
                validate_integrated(name, damaged, validate_dcm.validate)
            except ValueError:
                continue
            raise ValueError(f"Integrated test accepted invalid data: {name} {label}")
    print("PASS: fifteen integrated Trampoline/AutoAS negative result checks")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location("dcm_checker", CHECK)
    if spec is None or spec.loader is None:
        raise RuntimeError("Original Dcm validator cannot be imported")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    self_test(module)
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        return 0
    if len(sys.argv) != 1:
        raise ValueError("Usage: test-h743-trampoline-autoas.py [--self-test]")
    for kind, peer in CASES.items():
        scenario = generate_scenario(kind, peer)
        log = OUT / f"{kind}.log"
        with log.open("w") as output:
            subprocess.run(
                [
                    str(RENODE), "--disable-gui", "-P", "-1",
                    "--console", "--plain",
                    "-e", f"include @build/emu/h743-trampoline-autoas/{scenario.name}; quit",
                ],
                cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                check=True, timeout=110,
            )
        text, words = read_status(log)
        if "[ERROR]" in text or "[FATAL]" in text:
            raise ValueError(f"Renode reported errors in {kind}: {log}")
        validate_integrated(kind, words, module.validate)
        print(
            f"PASS real Trampoline+AutoAS {kind}: "
            f"OSEK 1ms={words[23]}, 10ms={words[24]}, "
            f"CAN physical transmissions={words[12]}"
        )
        print(f"  CPU evidence: {log}")
    print("PASS: one upstream Trampoline+AutoAS ELF served all three UDS cases")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as exc:
        print(f"FAIL: integrated Trampoline/AutoAS test: {exc}", file=sys.stderr)
        sys.exit(1)
