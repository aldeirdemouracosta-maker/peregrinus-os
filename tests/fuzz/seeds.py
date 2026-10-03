#!/usr/bin/env python3
"""Writes a small seed corpus of well-formed frames (first byte = direction)."""
import struct, sys, pathlib

out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
MAC_US = bytes.fromhex("525400123456"); MAC_PEER = bytes.fromhex("525400123499")
US = bytes([10, 0, 2, 15]); PEER = bytes([10, 0, 2, 2])

def csum(b):
    if len(b) % 2: b += b"\0"
    s = sum(struct.unpack(f"!{len(b)//2}H", b))
    while s >> 16: s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF

def ip(proto, payload, src=PEER, dst=US):
    h = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(payload), 1, 0, 64, proto, 0, src, dst)
    return h[:10] + struct.pack("!H", csum(h)) + h[12:] + payload

def eth(t, p, dst=MAC_US): return dst + MAC_PEER + struct.pack("!H", t) + p

icmp = struct.pack("!BBHHH", 8, 0, 0, 1, 1) + b"seed"
icmp = icmp[:2] + struct.pack("!H", csum(icmp)) + icmp[4:]
seeds = {
    "arp_req": b"\0" + eth(0x0806, struct.pack("!HHBBH6s4s6s4s", 1, 0x0800, 6, 4, 1, MAC_PEER, PEER, b"\0"*6, US), b"\xff"*6),
    "icmp_echo": b"\0" + eth(0x0800, ip(1, icmp)),
    "udp": b"\0" + eth(0x0800, ip(17, struct.pack("!HHHH", 4000, 53, 12, 0) + b"abcd")),
    "tcp_syn_out": b"\1" + eth(0x0800, ip(6, struct.pack("!HHIIBBHHH", 4000, 80, 1, 0, 0x50, 0x02, 1024, 0, 0), src=US, dst=PEER)),
    "vlan": b"\0" + MAC_US + MAC_PEER + b"\x81\x00\x00\x01\x08\x00",
}
for name, data in seeds.items(): (out / name).write_bytes(data)
