#!/usr/bin/env python3
"""With interrupts live, an idle shell must halt the CPU instead of spinning.

Measures the QEMU process CPU time over a quiet window (utime+stime from /proc) and checks the
`tempo` command reports a running timer. Usage: idle_check.py <serial.sock> <qemu_pid>"""
import os, socket, sys, time

sock_path, pid = sys.argv[1], int(sys.argv[2])
s = socket.socket(socket.AF_UNIX)
for _ in range(100):
    try: s.connect(sock_path); break
    except OSError: time.sleep(0.2)
s.settimeout(0.2)
log = b''
def read_until(tok, secs):
    global log
    end = time.time() + secs
    while time.time() < end and tok.encode() not in log:
        try: log += s.recv(4096)
        except socket.timeout: pass
    return tok.encode() in log
def cpu_seconds():
    f = open(f'/proc/{pid}/stat').read().rsplit(')', 1)[1].split()
    return (int(f[11]) + int(f[12])) / os.sysconf('SC_CLK_TCK')

if not read_until('peregrinus> ', 120): print('FAIL: no prompt'); sys.exit(1)
s.sendall(b'status\r')
if not read_until('Interrupções: ativas', 10): print('FAIL: interrupts not live'); print(log.decode('utf-8','replace')[-600:]); sys.exit(1)
time.sleep(2)
c0, t0 = cpu_seconds(), time.time()
time.sleep(6)
usage = (cpu_seconds() - c0) / (time.time() - t0)
print('idle QEMU CPU usage: %.0f%% of one core' % (usage * 100))
if usage > 0.5: print('FAIL: guest is spinning instead of halting'); sys.exit(1)
s.sendall(b'tempo\r')
if not read_until('Tempo desde o boot', 10): print('FAIL: tempo not answered'); sys.exit(1)
line = log[log.rfind(b'Tempo desde o boot'):].split(b'\n')[0].decode('utf-8', 'replace')
print(line.strip())
