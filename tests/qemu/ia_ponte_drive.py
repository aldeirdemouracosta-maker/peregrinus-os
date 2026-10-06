#!/usr/bin/env python3
"""Drive the ia-ponte profile in QEMU: the PS/2 keyboard types `pergunte`, the real bridge
(scripts/ia-ponte.py) answers through COM1 from a fake llama-server, and the answer must show
up sanitized; with the bridge gone, Esc must cancel the wait.
Usage: ia_ponte_drive.py <bridge.log> <monitor.sock> <screendump.ppm> <bridge pid>"""
import os, signal, socket, sys, time

log_path, mon_path, dump, bridge_pid = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])


def log():
    try:
        return open(log_path, encoding="utf-8", errors="replace").read()
    except OSError:
        return ""


def wait_for(text, seconds, start=0):
    end = time.time() + seconds
    while time.time() < end:
        if text in log()[start:]:
            return True
        time.sleep(0.2)
    return text in log()[start:]


def fail(msg):
    sys.stdout.write(log()[-1500:]); print("\nFAIL:", msg); sys.exit(1)


mon = socket.socket(socket.AF_UNIX)
for _ in range(100):
    try:
        mon.connect(mon_path); break
    except OSError:
        time.sleep(0.2)
mon.settimeout(0.5)


def keys(seq):
    for k in seq:
        mon.sendall(("sendkey %s\n" % k).encode()); time.sleep(0.12)
        try: mon.recv(65536)
        except socket.timeout: pass


def word(w):
    return ["spc" if c == " " else "minus" if c == "-" else c for c in w]


if not wait_for("peregrinus> ", 90): fail("no shell prompt through the bridge")
print("ok: shell prompt reached through the bridge")

mark = len(log())
keys(word("pergunte capital") + ["ret"])
if not wait_for("Resposta de teste: capital (historico=2)", 20, mark): fail("answer to `pergunte capital` not printed")
print("ok: keyboard `pergunte` -> COM1 frame -> bridge -> fake llama-server -> answer printed by the kernel")

mark = len(log())
keys(word("pergunte -n lixo") + ["ret"])
if not wait_for('"aspas" - fim... emoji ? C1 ok  tab', 20, mark): fail("sanitized answer not printed")
tail = log()[mark:]
if "\x1b" in tail or "raciocinio" in tail or "\x02" in tail: fail("control bytes or hidden reasoning reached the console")
if "pergunta #2 (nova conversa): lixo" not in tail: fail("`-n` did not start a new conversation")
if "\nação\n" not in tail.replace("\r", ""): fail("multi-byte text garbled between the kernel and the terminal")
print("ok: `pergunte -n` starts a new conversation; escape sequences, C1 controls and <think> removed")

os.kill(bridge_pid, signal.SIGTERM); time.sleep(0.5)
keys(word("pergunte sem ponte") + ["ret"])
time.sleep(1.5)
keys(["esc"])
time.sleep(1.0)
mon.sendall(("screendump %s\n" % dump).encode()); time.sleep(1.5)
print("ok: bridge stopped; Esc pressed while waiting")
