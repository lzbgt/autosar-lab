#!/usr/bin/env python3
"""Rootless UDP frame relay; deliberately not a physical CAN arbitration/error simulator."""
import argparse
import signal
import socket
import struct
import time

WIRE_BYTES = 72
FD_LENS = set(range(9)) | {12, 16, 20, 24, 32, 48, 64}


def valid_frame(data: bytes) -> bool:
    if len(data) != WIRE_BYTES:
        return False
    identifier, length, flags, r1, r2 = struct.unpack_from("<IBBBB", data)
    return (identifier <= 0x7FF and flags <= 3 and r1 == 0 and r2 == 0
            and length in (FD_LENS if flags & 1 else range(9)))


def serve(port: int, verbose: bool) -> None:
    peers: dict[tuple[str, int], float] = {}
    running = True

    def shutdown(_sig, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", port))
        sock.settimeout(0.25)
        print(f"Virtual CAN relay listening on 127.0.0.1:{port}", flush=True)
        while running:
            try:
                data, sender = sock.recvfrom(4096)
            except socket.timeout:
                continue
            now = time.monotonic()
            if data == b"R":
                peers[sender] = now
                continue
            if sender not in peers or not valid_frame(data):
                continue
            peers[sender] = now
            if verbose:
                identifier, length, flags = struct.unpack_from("<IBB", data)
                print(f"frame {identifier:#05x} len={length} fd={flags & 1}", flush=True)
            for peer, seen in list(peers.items()):
                if now - seen > 30.0:
                    peers.pop(peer, None)
                elif peer != sender:
                    try:
                        sock.sendto(data, peer)
                    except OSError:
                        peers.pop(peer, None)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port out of range")
    serve(args.port, args.verbose)
