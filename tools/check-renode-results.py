#!/usr/bin/env python3
"""Validate Renode firmware effects, not merely emulator process exit."""
from pathlib import Path
import re
import sys

def readings(path: str) -> tuple[str, list[int]]:
    body = Path(path).read_text(errors="replace").replace("\r", "")
    vals = [int(v, 16) for v in re.findall(
        r"(?m)^\s*0x([0-9A-Fa-f]{8})\s*$", body)]
    return body, vals

def main() -> int:
    if len(sys.argv) != 3 or sys.argv[1] not in ("h743-fdcan", "f407-trampoline"):
        print("Usage: check-renode-results.py h743-fdcan|f407-trampoline RENODE_LOG", file=sys.stderr)
        return 2
    body, vals = readings(sys.argv[2])
    if "[ERROR]" in body:
        raise ValueError("Renode log contains emulator errors")
    if sys.argv[1] == "h743-fdcan":
        if len(vals) != 6:
            raise ValueError(f"Expected exactly 6 firmware state reads, got {vals!r}")
        sender, sender_crel, sender_tx, receiver, rx_id, rx_header = vals
        if sender != 0xCAFECC08 or receiver != 0xCAFEFD10:
            raise ValueError(
                f"Real CPU firmware did not complete CAN FD+Classical exchange: "
                f"sender={sender:#010x}, receiver={receiver:#010x}")
        if ((rx_id >> 18) & 0x7FF) != 0x201:
            raise ValueError(f"Incorrect received CAN ID in M_CAN header: {rx_id:#010x}")
        if ((rx_header >> 16) & 0xF) != 10:
            raise ValueError("Expected FD data length code 10 (16 bytes)")
        if (rx_header & (3 << 20)) != (3 << 20):
            raise ValueError("Expected FDF=1, BRS=1 from actual receiver RAM")
        if sender_crel == 0 or not (sender_tx & 1):
            raise ValueError("Emulated M_CAN did not expose revision/Tx complete")
        print(
            "PASS: two Cortex-M7 STM32H743 machines, FD 16 bytes DLC=10 "
            "FDF=1/BRS=1 id=0x201, Classical CAN 8-byte echo id=0x202")
        print(f"PASS: receiver {receiver:#010x}, sender {sender:#010x}, TxComplete {sender_tx:#x}")
    else:
        if len(vals) != 9:
            raise ValueError(f"Expected 9 sampled GPIO readings, got {len(vals)}")
        states = [(x >> 15) & 1 for x in vals]
        transitions = sum(x != y for x, y in zip(states, states[1:]))
        if transitions < 3:
            raise ValueError(f"Insufficient Trampoline periodic task activations: {states}")
        print(f"PASS: real Trampoline Cortex-M4 OS firmware changed STM32F407 GPIO D15 "
              f"{transitions} times over 2.7s emulated time")
        print(f"PASS: GPIO D15 states: {''.join(str(i) for i in states)}")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
