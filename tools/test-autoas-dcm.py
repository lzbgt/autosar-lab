#!/usr/bin/env python3
"""Execute three real Dcm services over isolated Renode STM32H743 CANHub runs.

Builds are done by tools/build-h743-autoas-dcm.sh. Every tester/server pair
starts with fresh SRAM/diagnostic session state. No host vcan privileges.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/emu/h743-autoas-dcm"
TEMPLATE = ROOT / "hardware/simulator/h743-autoas-dcm/two-node.resc"
RENODE = ROOT / "third_party/renode-1.17.0/renode"
CHECK = ROOT / "tools/check-autoas-dcm.py"
DEFAULT_TESTER = "@build/emu/h743-autoas-tp/tester.elf"
KINDS = {
    "did": DEFAULT_TESTER,
    "session": "@build/emu/h743-autoas-dcm/tester-session.elf",
    "reject": "@build/emu/h743-autoas-dcm/tester-reject.elf",
}


def main() -> int:
    if not RENODE.is_file():
        raise RuntimeError("Install Renode first: bash tools/install-renode.sh")
    source = TEMPLATE.read_text()
    if source.count(DEFAULT_TESTER) != 1:
        raise RuntimeError("Renode template no longer contains exactly one tester ELF")
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(CHECK), "--self-test"], check=True)
    for kind, image in KINDS.items():
        scenario = OUT / f"scenario-{kind}.resc"
        scenario.write_text(source.replace(DEFAULT_TESTER, image))
        log = OUT / f"{kind}.log"
        cmd = [
            str(RENODE), "--disable-gui", "-P", "-1", "--console", "--plain",
            "-e", f"include @build/emu/h743-autoas-dcm/scenario-{kind}.resc; quit",
        ]
        with log.open("w") as fd:
            subprocess.run(
                cmd, cwd=ROOT, stdout=fd, stderr=subprocess.STDOUT,
                check=True, timeout=95
            )
        subprocess.run([sys.executable, str(CHECK), kind, str(log)], check=True)
    print("PASS: three genuine AutoAS Dcm diagnostic service cases on STM32H743")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        print(f"FAIL: real Dcm Renode regression: {exc}", file=sys.stderr)
        sys.exit(1)
