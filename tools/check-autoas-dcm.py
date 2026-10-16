#!/usr/bin/env python3
"""Verify genuine AutoAS Dcm transactions from TWO Cortex-M7 SRAM snapshots.

Testers compare the *entire* UDS payload in their real application code.
The server-side statuses are based on actual Dcm DID/session callbacks and
successful CanTp transmit confirmation. Frame-class counters are sampled
by the M_CAN driver before passing the frame to CanIf.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys

# Each snapshot: [status, completed_app_PDUs, frame_tx_confirms, DET_errors,
# completed_tp_transmits, raw_SF, raw_FF, raw_CF, raw_FC, session_changes].
# The tuple values are independent, read via Renode sysbus.
EXPECTED: dict[str, tuple[tuple[int, ...], tuple[int, ...]]] = {
    "did": (
        (0xCAFE220D, 1, 2, 0, 1, 0, 1, 1, 0, 0),
        (0xCAFE22DC, 1, 2, 0, 1, 1, 0, 0, 1, 0),
    ),
    "session": (
        (0xCAFE1003, 1, 1, 0, 1, 1, 0, 0, 0, 0),
        (0xCAFE1003, 0, 1, 0, 1, 1, 0, 0, 0, 1),
    ),
    "reject": (
        (0xCAFE2231, 1, 1, 0, 1, 1, 0, 0, 0, 0),
        (0xCAFE2231, 0, 1, 0, 1, 1, 0, 0, 0, 0),
    ),
}


def validate(kind: str, values: list[int]) -> None:
    if kind not in EXPECTED:
        raise ValueError(f"unknown Dcm service test: {kind}")
    if len(values) != 23:
        raise ValueError(f"expected 23 emulated SRAM words, observed {len(values)}")
    for label, actual, expected in (
        ("tester", values[:10], EXPECTED[kind][0]),
        ("real Dcm server", values[10:20], EXPECTED[kind][1]),
    ):
        for word, (observed, required) in enumerate(zip(actual, expected)):
            if observed != required:
                raise ValueError(
                    f"{kind} {label} SRAM[{word}]: "
                    f"expected 0x{required:08X}, observed 0x{observed:08X}"
                )


    # Measured 1.2 seconds of virtual time, after the MCU reset-state
    # 64 MHz HSI clock is used for SysTick; this is NOT physical clock proof.
    tick_irq, can_tp_fast, ten_ms_service = values[20:23]
    if not (1170 <= tick_irq <= 1230):
        raise ValueError(f"expected ~1200 1-ms SysTick interrupts, got {tick_irq}")
    if not (tick_irq - 5 <= can_tp_fast <= tick_irq):
        raise ValueError(f"1-ms CanTp task missed ticks: IRQ={tick_irq}, fast={can_tp_fast}")
    if ten_ms_service != can_tp_fast // 10:
        raise ValueError(
            f"10-ms CanTp/Dcm service count wrong: fast={can_tp_fast}, main={ten_ms_service}"
        )


def validate_file(kind: str, log: Path) -> None:
    text = log.read_text(errors="replace").replace("\r", "")
    if "[ERROR]" in text or "[FATAL]" in text:
        raise ValueError("Renode reported an emulator error")
    words = [
        int(value, 16)
        for value in re.findall(r"(?m)^\s*0x([0-9a-fA-F]{8})\s*$", text)
    ]
    validate(kind, words)


def self_test() -> None:
    for name, (client, server) in EXPECTED.items():
        raw = list(client + server) + [1200, 1199, 119]
        validate(name, raw)
        for desc, position, corrupted in (
            ("wrong status", 0, 0),
            ("missing raw SF", 5 if name != "did" else 6, 0),
            ("spurious development error", 13, 1),
            ("missing tx confirmation", 12, 0),
            ("wrong SysTick period", 20, 159),
            ("lost fast timer task", 21, 900),
            ("wrong 10-ms task count", 22, 12),
        ):
            damaged = raw.copy()
            damaged[position] = corrupted
            try:
                validate(name, damaged)
            except ValueError:
                pass
            else:
                raise ValueError(f"checker failed to reject {name}: {desc}")
    print("PASS: diagnostic result validator rejects bad statuses, frames and error counts")


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        self_test()
        return 0
    if len(sys.argv) != 3:
        print("Usage: check-autoas-dcm.py did|session|reject RENODE_LOG", file=sys.stderr)
        return 2
    kind = sys.argv[1]
    path = Path(sys.argv[2])
    validate_file(kind, path)
    descriptions = {
        "did": "UDS 22 F1 5B -> 62 F1 5B + ten A5 (13 bytes, FF/FC/CF)",
        "session": "UDS 10 03 -> 50 03 00 32 01 F4 (session change)",
        "reject": "UDS 22 F1 99 -> 7F 22 31 (Negative Response Code 0x31)",
    }
    print(f"PASS real AutoAS Dcm: {descriptions[kind]}")
    print("PASS: measured ~1200 hardware SysTick interrupts, matched 1-ms CanTp and 10-ms Dcm cycles")
    print(f"PASS independently validated Dcm/CanTp/PduR/CanIf and M_CAN counters: {path}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
