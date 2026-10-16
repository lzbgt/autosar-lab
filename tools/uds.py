#!/usr/bin/env python3
"""Single-request UDS tester using standard-library SocketCAN or rootless UDP relay."""
import argparse
import socket
import struct
import sys
import time

REQUEST_ID = 0x7E0
RESPONSE_ID = 0x7E8


class Channel:
    def __init__(self, backend: str, ifname: str, port: int):
        self.backend = backend
        if backend == "socketcan":
            self.sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
            self.sock.bind((ifname,))
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind(("127.0.0.1", 0))
            self.sock.connect(("127.0.0.1", port))
            self.sock.send(b"R")

    def close(self):
        self.sock.close()

    def send(self, can_id: int, payload: bytes):
        if not 1 <= len(payload) <= 8:
            raise ValueError("Expected 1..8 Classical CAN bytes")
        if self.backend == "socketcan":
            frame = struct.pack("=IB3x8s", can_id, len(payload), payload.ljust(8, b"\0"))
        else:
            frame = struct.pack("<IBBBB64s", can_id, len(payload), 0, 0, 0, payload.ljust(64, b"\0"))
        self.sock.send(frame)

    def receive(self, deadline: float) -> bytes:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("UDS response timeout")
            self.sock.settimeout(remaining)
            try:
                frame = self.sock.recv(128)
            except socket.timeout as error:
                raise TimeoutError("UDS response timeout") from error
            if self.backend == "socketcan":
                if len(frame) != 16:
                    continue
                can_id, length, payload = struct.unpack("=IB3x8s", frame)
                if can_id & ~0x7FF:
                    continue
            else:
                if len(frame) != 72:
                    continue
                can_id, length, flags, _a, _b, payload = struct.unpack("<IBBBB64s", frame)
                if flags & 1:
                    continue
            if can_id == RESPONSE_ID and 1 <= length <= 8:
                return payload[:length]


def request(channel: Channel, uds: bytes, block_size: int, timeout: float) -> bytes:
    if not 1 <= len(uds) <= 7:
        raise ValueError("Only single-frame requests are implemented")
    channel.send(REQUEST_ID, bytes((len(uds),)) + uds + bytes(7 - len(uds)))
    deadline = time.monotonic() + timeout
    first = channel.receive(deadline)
    pci = first[0] >> 4
    if pci == 0:
        size = first[0] & 0x0F
        if not 1 <= size <= min(7, len(first) - 1):
            raise ValueError("Malformed ISO-TP Single Frame")
        return first[1:1 + size]
    if pci != 1 or len(first) < 8:
        raise ValueError("Expected ISO-TP First Frame")
    size = ((first[0] & 0x0F) << 8) | first[1]
    if not 8 <= size <= 64:
        raise ValueError("Unsupported ISO-TP total length")
    assembled = bytearray(first[2:8])
    expected_sn = 1
    channel.send(REQUEST_ID, bytes((0x30, block_size, 0)) + bytes(5))
    count = 0
    while len(assembled) < size:
        fragment = channel.receive(deadline)
        if len(fragment) < 2 or fragment[0] >> 4 != 2:
            raise ValueError("Expected ISO-TP Consecutive Frame")
        if (fragment[0] & 0x0F) != expected_sn:
            raise ValueError("Incorrect Consecutive Frame sequence number")
        assembled.extend(fragment[1:])
        expected_sn = (expected_sn + 1) & 0x0F
        count += 1
        if block_size and count == block_size and len(assembled) < size:
            channel.send(REQUEST_ID, bytes((0x30, block_size, 0)) + bytes(5))
            count = 0
    return bytes(assembled[:size])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("udp", "socketcan"), default="udp")
    parser.add_argument("--ifname", default="vcan0")
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--block-size", type=int, default=1)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--session", type=lambda s: int(s, 0))
    group.add_argument("--did", type=lambda s: int(s, 0), default=0xF190)
    parser.add_argument("--expect-negative", type=lambda s: int(s, 0))
    args = parser.parse_args()
    if not 0 <= args.block_size <= 255 or not 0 < args.timeout <= 30:
        parser.error("Invalid block size or timeout")
    if args.session is not None:
        if not 0 <= args.session <= 255:
            parser.error("Session must be one byte")
        uds = bytes((0x10, args.session))
    else:
        if not 0 <= args.did <= 65535:
            parser.error("DID must fit two bytes")
        uds = bytes((0x22, args.did >> 8, args.did & 255))

    channel = Channel(args.backend, args.ifname, args.port)
    try:
        answer = request(channel, uds, args.block_size, args.timeout)
    finally:
        channel.close()

    print(f"REQ: {uds.hex(' ').upper()}")
    print(f"RES: {answer.hex(' ').upper()}")
    if args.expect_negative is not None:
        if answer != bytes((0x7F, uds[0], args.expect_negative)):
            raise ValueError("Unexpected negative response")
        print(f"PASS expected NRC 0x{args.expect_negative:02X}")
        return 0
    if answer[0] == 0x7F:
        print(f"UDS negative response NRC=0x{answer[2]:02X}", file=sys.stderr)
        return 2
    if answer[0] != uds[0] + 0x40 or (uds[0] == 0x22 and answer[1:3] != uds[1:3]):
        raise ValueError("Unexpected positive response")
    if uds == bytes((0x22, 0xF1, 0x90)):
        print(f"VIN: {answer[3:].decode('ascii')}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
