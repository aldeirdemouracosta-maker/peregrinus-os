#!/usr/bin/env python3
"""Ask the kernel's local LLM for a deterministic (greedy) completion over COM1 and compare it
with the host build of the same engine (itself checked against llama2.c run.c by tests/llm.sh).
Usage: llm_drive.py <serial.sock> <expected.txt> <prompt>"""
import socket, sys, time
sock_path, expected_path, prompt = sys.argv[1], sys.argv[2], sys.argv[3]
expected = open(expected_path, 'rb').read().rstrip(b'\n')
s = socket.socket(socket.AF_UNIX)
for _ in range(100):
    try: s.connect(sock_path); break
    except OSError: time.sleep(0.2)
s.settimeout(0.2)
log = b''
def read_until(tok, secs, start=0):
    global log
    end = time.time() + secs
    while time.time() < end and tok not in log[start:]:
        try: log += s.recv(65536)
        except socket.timeout: pass
    return tok in log[start:]
if not read_until(b'peregrinus> ', 120): print('FAIL: no prompt'); sys.exit(1)
m0 = len(log)
s.sendall(b'status\r')
if not read_until(b'peregrinus> ', 20, m0 + 1): print('FAIL: status'); sys.exit(1)
time.sleep(0.3)
try: log += s.recv(65536)
except socket.timeout: pass
mark = len(log)
s.sendall(b'conversa -g ' + prompt.encode('latin-1') + b'\r')
if not read_until(b'peregrinus> ', 180, mark): print('FAIL: no answer'); print(log[mark:][-800:]); sys.exit(1)
reply = log[mark:].split(b'\n', 1)[1]           # drop the echoed command line
reply = reply[:reply.rfind(b'peregrinus> ')].replace(b'\r\n', b'\n').rstrip(b'\n')
if reply != expected:
    print('FAIL: kernel answer differs from the host engine')
    print('kernel  :', reply[:200]); print('expected:', expected[:200]); sys.exit(1)
print('kernel greedy completion matches the host engine byte for byte (%d bytes)' % len(reply))
