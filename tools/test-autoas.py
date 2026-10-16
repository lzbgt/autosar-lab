#!/usr/bin/env python3
"""Run real AutoAS CanApp and IsoTpSend against upstream CAN simulator_v2.

No dependence on the local src/core teaching stack. POSIX/Linux only.
The simulator uses UDP multicast; run on a development machine, not a vehicle.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
AUTOAS = ROOT / "third_party" / "autoas"
BIN = AUTOAS / "build" / "posix" / "GCC"
APP = BIN / "CanApp" / "CanApp"
CLIENT = BIN / "IsoTpSend" / "IsoTpSend"
LIB = BIN / "prebuilt" / "libAsOne.so"
LOG = ROOT / "build" / "stack-evidence" / "autoas-ecu.log"
RESPONSES = {
    # DiagnosticSessionControl: default session 0x01 and extended 0x03
    "1001": bytes.fromhex("50 01 00 32 01 F4"),
    "1003": bytes.fromhex("50 03 00 32 01 F4"),
    # ReadDataByIdentifier 0xF15B: 13 UDS bytes require ISO-TP segmentation.
    "22F15B": bytes.fromhex("62 F1 5B") + bytes([0xA5] * 10),
}


def response_bytes(output: str) -> bytes | None:
    matches = re.findall(r"(?m)^RX:\s*([0-9A-Fa-f ]+)\s*$", output)
    if not matches:
        return None
    try:
        return bytes.fromhex(matches[-1])
    except ValueError:
        return None


def main() -> int:
    for path in (APP, CLIENT, LIB):
        if not path.is_file():
            print(f"Missing real stack artifact: {path}", file=sys.stderr)
            print("Build first: bash tools/build-autoas.sh", file=sys.stderr)
            return 2
    if shutil.which("stdbuf") is None:
        print("Missing GNU coreutils stdbuf; required for bounded ECU logs.", file=sys.stderr)
        return 2

    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = str(LIB.parent) + (
        os.pathsep + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else ""
    )
    # This upstream multicast simulator does not work with forced 127.0.0.1.
    # Clear inherited network-interface overrides for reproducible host tests.
    env.pop("AS_LOCAL_IP", None)
    LOG.parent.mkdir(parents=True, exist_ok=True)
    print("AutoAS real Classic BSW test (UDP multicast simulator_v2)")
    print(f"Real ECU: {APP}")
    print(f"Real transport tester: {CLIENT}")

    with LOG.open("w", encoding="utf-8") as output:
        ecu = subprocess.Popen(
            ["stdbuf", "-oL", "-eL", str(APP)],
            cwd=AUTOAS, env=env, stdin=subprocess.PIPE,
            stdout=output, stderr=subprocess.STDOUT, start_new_session=True,
        )
        try:
            time.sleep(1.0)
            if ecu.poll() is not None:
                raise RuntimeError(f"AutoAS ECU exited early (code {ecu.returncode})")
            for request, expected in RESPONSES.items():
                result = subprocess.run(
                    [str(CLIENT), "-d", "CAN.simulator_v2",
                     "-v", request, "-T", "2000"],
                    cwd=AUTOAS, env=env, text=True,
                    capture_output=True, timeout=12, check=False,
                )
                received = response_bytes(result.stdout)
                if received != expected:
                    raise RuntimeError(
                        f"UDS {request}: expected {expected.hex(' ').upper()}, "
                        f"received {None if received is None else received.hex(' ').upper()}, "
                        f"exit={result.returncode}\n{result.stdout}\n{result.stderr}"
                    )
                # Upstream IsoTpSend returns received-byte count, not POSIX success 0.
                if result.returncode != len(expected):
                    raise RuntimeError(
                        f"UDS {request}: IsoTpSend exit={result.returncode}, "
                        f"expected payload length {len(expected)}"
                    )
                print(
                    f"PASS UDS {request}: {received.hex(' ').upper()} "
                    f"({len(received)} bytes)", flush=True
                )
            print("PASS: real Dcm, PduR, CanTp, CanIf, simulated CAN controller", flush=True)
            print(f"ECU startup/error log: {LOG}", flush=True)
            return 0
        except (subprocess.TimeoutExpired, RuntimeError) as error:
            print(f"FAIL: {error}\nECU log: {LOG}", file=sys.stderr)
            return 1
        finally:
            # Keep the input pipe open while the ECU runs; otherwise upstream
            # shell handling floods 'shell input buffer overflow' on EOF.
            try:
                os.killpg(ecu.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                ecu.wait(timeout=4)
            except subprocess.TimeoutExpired:
                os.killpg(ecu.pid, signal.SIGKILL)
                ecu.wait(timeout=3)
            if ecu.stdin is not None:
                ecu.stdin.close()


if __name__ == "__main__":
    sys.exit(main())
