#!/usr/bin/env python3
"""Drive the QEMU e1000 qualification kernel through a `-netdev socket,udp=` link.

QEMU forwards each Ethernet frame as one UDP datagram, so this script acts as the only
other host on the wire. It checks the behaviour the Muro datapath claims:
ARP reply, ICMP echo reply, default-deny for UDP, and the per-source burst guard.
Exit status is non-zero on the first failed expectation.
"""
import socket, struct, sys, time

LOCAL = ('127.0.0.1', int(sys.argv[1]) if len(sys.argv) > 1 else 5556)
PEER = ('127.0.0.1', int(sys.argv[2]) if len(sys.argv) > 2 else 5555)
BOOT_WAIT = float(sys.argv[3]) if len(sys.argv) > 3 else 60.0

HMAC = bytes([0x52, 0x54, 0, 0xaa, 0xbb, 0xcc])
HIP = bytes([10, 0, 2, 2])
GIP = bytes([10, 0, 2, 15])
BCAST = b'\xff' * 6

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(LOCAL)
sock.settimeout(0.5)


def csum(b):
    if len(b) % 2:
        b += b'\0'
    s = sum(struct.unpack('!%dH' % (len(b) // 2), b))
    while s >> 16:
        s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff


def fail(msg):
    print('FAIL:', msg)
    sys.exit(1)


def drain():
    try:
        while True:
            sock.recvfrom(4096)
    except socket.timeout:
        pass


def recv_matching(pred, seconds):
    end = time.time() + seconds
    while time.time() < end:
        try:
            d, _ = sock.recvfrom(4096)
        except socket.timeout:
            continue
        if pred(d):
            return d
    return None


arp_req = BCAST + HMAC + b'\x08\x06' + struct.pack('!HHBBH', 1, 0x0800, 6, 4, 1) + HMAC + HIP + b'\0' * 6 + GIP
is_arp_reply = lambda d: d[12:14] == b'\x08\x06' and d[20:22] == b'\x00\x02' and d[28:32] == GIP

# The kernel boots while we wait: keep asking until ARP is answered.
end = time.time() + BOOT_WAIT
reply = None
while time.time() < end and reply is None:
    sock.sendto(arp_req, PEER)
    reply = recv_matching(is_arp_reply, 1.0)
if reply is None:
    fail('no ARP reply for 10.0.2.15 (datapath never started?)')
gmac = reply[22:28]
print('PASS: ARP reply from', gmac.hex(':'))


def ipv4(proto, payload, src=HIP, ident=1):
    # TTL 5 on purpose: replies must carry a fresh TTL, not echo the request's.
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + len(payload), ident, 0, 5, proto, 0, src, GIP)
    ip = ip[:10] + struct.pack('!H', csum(ip)) + ip[12:]
    return gmac + HMAC + b'\x08\x00' + ip + payload


def ping(seq, src=HIP):
    ic = struct.pack('!BBHHH', 8, 0, 0, 0x1234, seq) + b'peregrinus-qualify'
    ic = ic[:2] + struct.pack('!H', csum(ic)) + ic[4:]
    return ipv4(1, ic, src=src)


def is_echo_reply(seq):
    return lambda d: (d[12:14] == b'\x08\x00' and d[23] == 1 and d[34] == 0 and d[26:30] == GIP
                      and struct.unpack('!H', d[40:42])[0] == seq)


drain()
for seq in (1, 2, 3):
    sock.sendto(ping(seq), PEER)
    r = recv_matching(is_echo_reply(seq), 3.0)
    if r is None:
        fail('no ICMP echo reply for seq %d' % seq)
    if r[22] != 64:
        fail('echo reply TTL is %d, expected a fresh 64' % r[22])
print('PASS: ICMP echo replies (fresh TTL)')

udp = struct.pack('!HHHH', 5000, 7, 8, 0)
sock.sendto(ipv4(17, udp), PEER)
if recv_matching(lambda d: d[12:14] == b'\x08\x00', 1.5) is not None:
    fail('UDP to a closed service produced a reply')
print('PASS: UDP denied by default policy')

# Burst guard: one source flooding echo requests must be throttled.
drain()
FLOOD = 200
for i in range(FLOOD):
    sock.sendto(ping(1000 + i, src=bytes([10, 0, 2, 77])), PEER)
replies = 0
end = time.time() + 4
while time.time() < end:
    try:
        sock.recvfrom(4096)
        replies += 1
    except socket.timeout:
        pass
if replies >= FLOOD // 2:
    fail('burst guard did not throttle the flood (%d/%d replies)' % (replies, FLOOD))
print('PASS: burst guard throttled flood (%d/%d replies)' % (replies, FLOOD))

# A different, well-behaved source is still served after the flood.
sock.sendto(ping(4242), PEER)
if recv_matching(is_echo_reply(4242), 3.0) is None:
    fail('legitimate source starved after another source flooded')
print('PASS: other sources still served')
