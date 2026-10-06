#!/usr/bin/env python3
"""Host test of scripts/ia-ponte.py: a fake QEMU serial socket and a fake llama-server."""
import os, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE = os.path.join(ROOT, "scripts", "ia-ponte.py")
fails = 0


def check(ok, what):
    global fails
    if not ok:
        print("FAIL:", what); fails += 1


def read_frame(conn, seconds=15):
    buf, end = b"", time.time() + seconds
    conn.settimeout(0.2)
    while time.time() < end:
        if b"\x03" in buf:
            start = buf.index(b"\x02")
            return buf[start + 1:buf.index(b"\x03", start)]
        try:
            chunk = conn.recv(65536)
            if not chunk:
                break
            buf += chunk
        except socket.timeout:
            pass
    return None


def ask(conn, seq, kind, text):
    conn.sendall(b"\x02PGQ1 %d %s\n" % (seq, kind) + text.encode("utf-8") + b"\x03\r\n")
    f = read_frame(conn)
    if f is None:
        return None, None
    head, _, body = f.partition(b"\n")
    return head.decode(), body.decode("utf-8")


with tempfile.TemporaryDirectory() as d:
    # Non-loopback servers are refused before anything is opened.
    r = subprocess.run([sys.executable, BRIDGE, "--server", "http://192.168.0.10:8080", "--serial", "unix:" + d + "/x", "--no-stdin", "--wait", "0"],
                       capture_output=True, text=True)
    check(r.returncode != 0 and "deve ser local" in r.stderr, "remote AI server refused")
    r = subprocess.run([sys.executable, BRIDGE, "--server", "https://127.0.0.1:8080", "--no-stdin", "--wait", "0"], capture_output=True, text=True)
    check(r.returncode != 0 and "servidor inválido" in r.stderr, "non-http URL refused")

    srv = subprocess.Popen([sys.executable, os.path.join(ROOT, "tests", "ia_ponte_fake_server.py")], stdout=subprocess.PIPE, text=True)
    port = int(srv.stdout.readline())
    path = os.path.join(d, "serial.sock")
    lsock = socket.socket(socket.AF_UNIX); lsock.bind(path); lsock.listen(1)
    log = open(os.path.join(d, "bridge.log"), "w+")
    br = subprocess.Popen([sys.executable, BRIDGE, "--serial", "unix:" + path, "--server", f"http://127.0.0.1:{port}", "--no-stdin", "--history", "2"],
                          stdout=log, stderr=subprocess.STDOUT)
    lsock.settimeout(15); conn, _ = lsock.accept()
    try:
        conn.sendall("peregrinus> lixo de log com acentuação\r\n".encode())
        head, body = ask(conn, 1, b"N", "Olá, qual a capital?")
        check(head == "PGA1 1 OK" and body == "Resposta de teste: Olá, qual a capital? (historico=2)", f"first answer [{head}] [{body}]")
        head, body = ask(conn, 2, b"C", "de novo")
        check(head == "PGA1 2 OK" and body.endswith("(historico=4)"), f"conversation history kept [{body}]")
        for i in range(3):
            ask(conn, 3 + i, b"C", f"mais {i}")
        head, body = ask(conn, 6, b"C", "limite")
        check(body.endswith("(historico=6)"), f"history bounded to 2 exchanges [{body}]")
        head, body = ask(conn, 7, b"N", "lixo")
        check(head == "PGA1 7 OK", "sanitized answer header")
        check(all(c == "\n" or (0x20 <= ord(c) < 0x7F) or 0xA0 <= ord(c) <= 0xFF for c in body), f"only printable Latin-1 [{body!r}]")
        check("raciocinio" not in body and '"aspas"' in body and " - fim..." in body and "\n\n\n" not in body and body.endswith("ação"), f"cleanup [{body!r}]")
        head, body = ask(conn, 8, b"C", "longo")
        check(head == "PGA1 8 OK" and len(body.encode()) <= 2000 and body.endswith("[...]"), "long answer cut at 2000 bytes")
        conn.sendall(b"\x02PGQ1 lixo\nx\x03")            # malformed: ignored, no reply
        srv.terminate(); srv.wait()
        head, body = ask(conn, 9, b"C", "servidor caiu?")
        check(head == "PGA1 9 ER" and "inacess" in body, f"server down -> ER frame [{head}] [{body}]")
    finally:
        conn.close(); lsock.close()
        try:
            br.wait(timeout=10)
        except subprocess.TimeoutExpired:
            br.kill(); check(False, "bridge did not exit when the serial closed")
        if srv.poll() is None:
            srv.terminate()
    log.seek(0); text = log.read()
    check("lixo de log com acentuação" in text, "console log mirrored to the terminal")
    check("quadro inválido ignorado" in text and "a serial fechou" in text, "malformed frame and close reported")
    check(br.returncode == 0, "bridge exit code")
sys.exit(1 if fails else 0)
