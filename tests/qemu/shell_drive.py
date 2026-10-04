#!/usr/bin/env python3
"""Drive the Peregrinus shell in QEMU through both input paths.

Serial: COM1 is a QEMU unix socket; we read the log and type a command into it.
Keyboard: QEMU monitor `sendkey` presses keys on the emulated PS/2 keyboard.
Usage: shell_drive.py <serial.sock> <monitor.sock> <screendump.ppm>"""
import socket, sys, time

ser_path, mon_path, dump = sys.argv[1:4]

def connect(path, tries=100):
    for _ in range(tries):
        try:
            s = socket.socket(socket.AF_UNIX); s.connect(path); return s
        except OSError:
            time.sleep(0.2)
    raise SystemExit('FAIL: cannot connect to ' + path)

ser = connect(ser_path); ser.settimeout(0.2)
mon = connect(mon_path); mon.settimeout(0.5)
log = b''

def read_until(token, seconds):
    global log
    end = time.time() + seconds
    while time.time() < end:
        if token.encode() in log:
            return True
        try:
            chunk = ser.recv(4096)
            if chunk:
                log += chunk
        except socket.timeout:
            pass
    return token.encode() in log

def monitor(cmd):
    mon.sendall((cmd + '\n').encode()); time.sleep(0.15)
    try: mon.recv(65536)
    except socket.timeout: pass

def fail(msg):
    sys.stdout.write(log.decode('utf-8', 'replace')[-1500:])
    print('\nFAIL:', msg); sys.exit(1)

if not read_until('peregrinus> ', 90): fail('no shell prompt')
print('ok: shell prompt reached')

# Serial input path.
mark = len(log)
ser.sendall(b'status\r')
if not read_until('Guard: BOOT-PASSIVE', 10): fail('serial command "status" not answered')
print('ok: serial input -> status')

# PS/2 keyboard path (sendkey uses QEMU key names).
for key in ['s', 'o', 'b', 'r', 'e', 'ret']:
    monitor('sendkey ' + key)
if not read_until('Geração', 10): fail('keyboard command "sobre" not answered')
print('ok: PS/2 keyboard -> sobre')

for key in ['x', 'y', 'z', 'backspace', 'backspace', 'backspace', 'h', 'w', 'ret']:
    monitor('sendkey ' + key)
if not read_until('RAM utiliz', 10): fail('keyboard backspace/hw failed')
print('ok: backspace editing -> hw')

# ABNT2 (default layout): the key right of L is ç; [ is the dead acute; ' is the dead tilde.
for key in ['t', 'e', 'c', 'l', 'a', 'd', 'o', 'ret']:
    monitor('sendkey ' + key)
if not read_until('Teclado atual: ABNT2', 10): fail('layout query failed')
for key in ['semicolon', 'bracket_left', 'a', 'apostrophe', 'o', 'ret']:
    monitor('sendkey ' + key)
if not read_until('Comando desconhecido: çáõ', 10): fail('ABNT2 ç / dead keys failed')
print('ok: ABNT2 keyboard: ç and dead keys (á, õ)')

time.sleep(0.5)
monitor('screendump ' + dump)
time.sleep(1.0)

for key in ['p', 'a', 'r', 'a', 'r', 'ret']:
    monitor('sendkey ' + key)
if not read_until('Sistema parado', 10): fail('halt command not answered')
print('ok: parar halts the system')
