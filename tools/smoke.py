#!/usr/bin/env python3
"""End-to-end integration with live ECU + UDS: Classical and CAN FD telemetry."""
import argparse
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time


def run_test(args):
    root = Path(__file__).resolve().parent.parent
    binary = root / args.build_dir / "lab_ecu"
    if not binary.is_file():
        raise RuntimeError(f"Build first: cmake -S . -B {args.build_dir} && cmake --build {args.build_dir}")
    if args.backend == "udp":
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.bind(("127.0.0.1", 0))
    else:
        probe = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        probe.bind((args.ifname,))
    probe.settimeout(0.3)
    server = None
    ecu = None
    try:
        if args.backend == "udp":
            server = subprocess.Popen([sys.executable, str(root / "tools/virtual_bus.py"),
                                       "--port", str(args.port)], stdout=subprocess.DEVNULL)
            time.sleep(0.1)
            if server.poll() is not None:
                raise RuntimeError("Virtual bus failed to start, perhaps port is occupied")
            probe.sendto(b"R", ("127.0.0.1", args.port))
        ecu = subprocess.Popen([str(binary), "--backend", args.backend, "--ifname", args.ifname,
                                "--port", str(args.port), "--duration", "5"] +
                               (["--fd"] if args.fd else []),
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        seen = False
        deadline = time.monotonic() + 2.5
        while time.monotonic() < deadline:
            try:
                frame = probe.recv(128)
            except socket.timeout:
                continue
            if args.backend == "udp" and len(frame) == 72:
                can_id, length, flags, _, _, _ = struct.unpack("<IBBBB64s", frame)
                seen = can_id == 0x201 and length == (16 if args.fd else 8) and bool(flags & 1) == args.fd
            elif args.backend == "socketcan" and len(frame) == (72 if args.fd else 16):
                seen = (struct.unpack_from("=I", frame)[0] & 0x7FF) == 0x201
            if seen:
                break
        if not seen:
            raise RuntimeError("No matching periodic speed frames observed")
        for options in [
            ["--session", "3"],
            ["--did", "0xF187"],
            ["--did", "0xF190", "--block-size", "1"],
            ["--did", "0xF199", "--expect-negative", "0x31"]
        ]:
            cli = [sys.executable, str(root / "tools/uds.py"), "--backend", args.backend,
                   "--ifname", args.ifname, "--port", str(args.port), *options]
            result = subprocess.run(cli, capture_output=True, text=True, timeout=5)
            if result.returncode:
                raise RuntimeError(f"UDS tester failed: {result.stdout} {result.stderr}")
            print(result.stdout.strip())
        if ecu.poll() is not None and ecu.returncode != 0:
            raise RuntimeError(f"ECU failed: {ecu.stderr.read()}")
        print("PASS telemetry, diagnostic session, DIDs, ISO-TP FF/FC/CF, negative response")
    finally:
        probe.close()
        for process in (ecu, server):
            if process is not None:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.communicate(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate(timeout=2)


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--backend", choices=("udp", "socketcan"), default="udp")
    cli.add_argument("--ifname", default="vcan0")
    cli.add_argument("--port", type=int, default=18765)
    cli.add_argument("--build-dir", default="build")
    cli.add_argument("--fd", action="store_true")
    args = cli.parse_args()
    try:
        run_test(args)
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        sys.exit(f"FAIL: {error}")
