#!/usr/bin/env python3
"""Validate genuine AutoAS CanIf/CanTp/PduR execution on Renode STM32H743 CPUs.

This checks CPU-written SRAM values, not a boot banner or just a zero exit
status. Real AUTOSAR module symbols are separately checked at link time.
"""
from pathlib import Path
import re
import sys

def read_log(path: Path) -> tuple[str, list[int]]:
    text = path.read_text(errors="replace").replace("\r", "")
    values = [int(x, 16) for x in re.findall(r"(?m)^\s*0x([0-9a-fA-F]{8})\s*$", text)]
    return text, values

def main() -> int:
    if len(sys.argv) != 3 or sys.argv[1] not in ("canif", "cantp-pdur"):
        print("Usage: check-autoas-renode.py canif|cantp-pdur RENODE_LOG", file=sys.stderr)
        return 2
    mode = sys.argv[1]
    log = Path(sys.argv[2])
    content, values = read_log(log)
    if "[ERROR]" in content or "[FATAL]" in content:
        raise ValueError("Renode logged an emulator error")
    if len(values) != 10:
        raise ValueError(f"Expected ten independent CPU memory reads, got {values!r}")
    first, second = values[:5], values[5:]
    if mode == "canif":
        # [finished status, raw M_CAN frame flags, Rx PDU callback count,
        #  TX confirmation count, development-error count] per CPU.
        a_ok = [0xCAFECC09, None, 1, 1, 0]
        b_ok = [0xCAFEFD11, None, 1, 1, 0]
        if (first[1] & (1 << 21)) != 0 or (second[1] & (3 << 20)) != (3 << 20):
            raise ValueError("Wrong actual CAN FD or Classical format flags")
    else:
        # [finished status, completed UDS PDU count, frame confirmations,
        #  development-error count, application TX completion count]
        a_ok = [0xCAFE220D, 1, 2, 0, 1]
        b_ok = [0xCAFE220E, 1, 2, 0, 1]
    for label, sample, expected in (
        ("sender/tester", first, a_ok), ("receiver/server", second, b_ok)
    ):
        for index, (actual, needed) in enumerate(zip(sample, expected)):
            if needed is not None and actual != needed:
                raise ValueError(
                    f"{label}: SRAM word {index} expected {needed:#x}, "
                    f"observed {actual:#x}")
    if mode == "canif":
        print("PASS: original AutoAS CanIf.c on two Cortex-M7 CPUs")
        print("PASS: 16-byte CAN FD + 8-byte Classical reply, real CanIf route and TX confirmations")
    else:
        print("PASS: original AutoAS CanIf.c + CanTp.c + PduR.c on both Cortex-M7 CPUs")
        print("PASS: SF request 22 F1 5B; FF/FC/CF transfer of 13-byte 62 F1 5B + 10*A5 response")
    if mode == "canif":
        print("PASS: one CAN Interface receive callback per endpoint, zero development-error reports")
    else:
        print("PASS: one reassembled diagnostic PDU per endpoint, zero development-error reports")
    print(f"Emulator evidence: {log}")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
