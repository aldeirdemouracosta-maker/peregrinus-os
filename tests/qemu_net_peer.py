#!/usr/bin/env python3
"""Network peer for the e1000 QEMU qualification boot.

QEMU is started with `-netdev socket,listen=127.0.0.1:PORT`; that backend
carries Ethernet frames over TCP, each prefixed by a 4-byte big-endian length.
This peer plays the 10.0.2.2 host and checks the Peregrinus datapath:

  1. ARP request for 10.0.2.15            -> ARP reply from the guest MAC
  2. ICMP echo request to 10.0.2.15       -> ICMP echo reply, same id/seq/payload
  3. UDP datagram to 10.0.2.15 (not allowlisted) -> no reply (default deny)

Exit code 0 only if all three hold.
"""
import socket
import struct
import sys
import time

GUEST_IP = bytes([10, 0, 2, 15])
PEER_IP = bytes([10, 0, 2, 2])
PEER_MAC = bytes.fromhex("525400123499")
BROADCAST = b"\xff" * 6


def checksum(data: bytes) -> int:
    if len(data) % 2:
        data += b"\0"
    s = sum(struct.unpack(f"!{len(data) // 2}H", data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def ipv4(proto: int, payload: bytes) -> bytes:
    hdr = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(payload), 0x4242, 0, 64, proto, 0, PEER_IP, GUEST_IP)
    hdr = hdr[:10] + struct.pack("!H", checksum(hdr)) + hdr[12:]
    return hdr + payload


def arp_request() -> bytes:
    arp = struct.pack("!HHBBH6s4s6s4s", 1, 0x0800, 6, 4, 1, PEER_MAC, PEER_IP, b"\0" * 6, GUEST_IP)
    return BROADCAST + PEER_MAC + b"\x08\x06" + arp


def icmp_echo(dst_mac: bytes, ident: int, seq: int, payload: bytes) -> bytes:
    body = struct.pack("!BBHHH", 8, 0, 0, ident, seq) + payload
    body = body[:2] + struct.pack("!H", checksum(body)) + body[4:]
    return dst_mac + PEER_MAC + b"\x08\x00" + ipv4(1, body)


def udp(dst_mac: bytes) -> bytes:
    body = struct.pack("!HHHH", 40000, 9, 8 + 4, 0) + b"deny"
    return dst_mac + PEER_MAC + b"\x08\x00" + ipv4(17, body)


class Link:
    def __init__(self, port: int, deadline: float):
        while True:
            try:
                self.sock = socket.create_connection(("127.0.0.1", port), timeout=2)
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise SystemExit("FAIL: could not connect to QEMU socket netdev")
                time.sleep(0.5)
        self.sock.settimeout(0.5)
        self.buf = b""

    def send(self, frame: bytes):
        frame = frame.ljust(60, b"\0")
        self.sock.sendall(struct.pack("!I", len(frame)) + frame)

    def recv(self):
        try:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise SystemExit("FAIL: QEMU closed the socket netdev")
            self.buf += chunk
        except socket.timeout:
            pass
        frames = []
        while len(self.buf) >= 4:
            (n,) = struct.unpack("!I", self.buf[:4])
            if len(self.buf) < 4 + n:
                break
            frames.append(self.buf[4:4 + n])
            self.buf = self.buf[4 + n:]
        return frames


def main() -> int:
    port = int(sys.argv[1])
    budget = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0
    deadline = time.monotonic() + budget
    link = Link(port, deadline)

    # 1. ARP: the guest only polls after boot, so keep asking until it answers.
    guest_mac = None
    while guest_mac is None:
        if time.monotonic() > deadline:
            print("FAIL: no ARP reply from 10.0.2.15")
            return 1
        link.send(arp_request())
        for f in link.recv():
            if len(f) >= 42 and f[12:14] == b"\x08\x06":
                op, sha, spa, tha, tpa = struct.unpack("!6xH6s4s6s4s", f[14:42])
                if op == 2 and spa == GUEST_IP and tpa == PEER_IP and f[0:6] == PEER_MAC:
                    guest_mac = sha
    print(f"PASS: ARP reply, guest MAC {guest_mac.hex(':')}")

    # 2. ICMP echo.
    payload = b"peregrinus-qualification"
    ident, seq = 0x5045, 1
    got_echo = False
    tries_deadline = time.monotonic() + 15
    while not got_echo:
        if time.monotonic() > tries_deadline:
            print("FAIL: no ICMP echo reply")
            return 1
        link.send(icmp_echo(guest_mac, ident, seq, payload))
        for f in link.recv():
            if len(f) >= 42 and f[12:14] == b"\x08\x00" and f[23] == 1:
                ip = f[14:34]
                icmp = f[34:]
                t, _, _, rid, rseq = struct.unpack("!BBHHH", icmp[:8])
                if (t == 0 and rid == ident and rseq == seq and ip[12:16] == GUEST_IP
                        and ip[16:20] == PEER_IP and icmp[8:8 + len(payload)] == payload
                        and checksum(ip) == 0 and checksum(icmp[:8 + len(payload)]) == 0):
                    got_echo = True
    print("PASS: ICMP echo reply (id/seq/payload/checksums verified)")

    # 3. UDP must be denied: send a few and make sure nothing UDP comes back.
    for _ in range(3):
        link.send(udp(guest_mac))
    quiet_until = time.monotonic() + 3
    while time.monotonic() < quiet_until:
        for f in link.recv():
            # Late duplicate echo replies are fine; any UDP or ICMP error is not.
            if len(f) >= 35 and f[12:14] == b"\x08\x00" and (f[23] == 17 or (f[23] == 1 and f[34] != 0)):
                print("FAIL: guest answered a non-allowlisted UDP datagram")
                return 1
    print("PASS: non-allowlisted UDP received no reply (default deny)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
