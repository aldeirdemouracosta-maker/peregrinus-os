#!/usr/bin/env python3
"""Ponte de IA: liga o shell do Peregrinus OS (perfil ia-ponte) a um servidor de IA no Linux.

O Peregrinus roda numa VM (QEMU/KVM) cuja COM1 é um socket. Esta ponte lê esse socket:
  - o log/console do Peregrinus aparece neste terminal (e o que você digitar aqui vai para o
    shell, como se fosse a serial);
  - cada `pergunte <texto>` chega como um quadro "\\x02PGQ1 <seq> <N|C>\\n<texto>\\x03\\n";
    a ponte pergunta ao llama-server (API compatível com OpenAI, só em 127.0.0.1/::1) e devolve
    "\\x02PGA1 <seq> <OK|ER>\\n<resposta>\\x03".

A resposta é convertida para texto simples que o console do Peregrinus desenha (Latin-1): sem
caracteres de controle, sem sequências de escape, no máximo 2000 bytes. O kernel sanitiza de
novo e só imprime: nada do que a IA responde é executado.

Uso:  scripts/ia-ponte.py --serial unix:/tmp/peregrinus-serial.sock --server http://127.0.0.1:8080
"""
import argparse
import codecs
import ipaddress
import json
import os
import re
import select
import socket
import sys
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request

STX, ETX = 0x02, 0x03
MAX_FRAME = 1200       # bytes from STX to ETX (the kernel sends at most ~340)
MAX_ANSWER = 2000      # UTF-8 bytes sent back (the kernel keeps at most 2048)
SYSTEM_PROMPT = (
    "Você é o assistente do Peregrinus OS, um sistema operacional micro e seguro. "
    "Responda em português do Brasil, de forma curta e direta (no máximo 12 linhas), "
    "em texto simples: sem Markdown, sem tabelas e sem emojis."
)
# Typographic characters the console cannot draw, mapped to close Latin-1/ASCII equivalents.
PUNCT = {
    "‘": "'", "’": "'", "‚": "'", "“": '"', "”": '"', "„": '"',
    "–": "-", "—": "-", "−": "-", "…": "...", "•": "*", " ": " ",
    "→": "->", "←": "<-", "≤": "<=", "≥": ">=", "≠": "!=", "­": "",
}
THINK = re.compile(r"<think>.*?</think>", re.S)


def to_console_text(text: str) -> str:
    """Model output -> printable Latin-1 text with '\\n' only, cut at MAX_ANSWER UTF-8 bytes."""
    text = THINK.sub("", text)
    text = unicodedata.normalize("NFC", text)
    out = []
    for ch in text:
        ch = PUNCT.get(ch, ch)
        for c in ch:
            o = ord(c)
            if c == "\n":
                out.append(c)
            elif c == "\t":
                out.append(" ")
            elif o < 0x20 or 0x7F <= o < 0xA0:
                continue                                   # controls, escape sequences, C1
            elif o <= 0xFF:
                out.append(c)
            else:
                ascii_ = unicodedata.normalize("NFKD", c).encode("ascii", "ignore").decode()
                out.append(ascii_ if ascii_ else "?")
    clean = re.sub(r"\n{3,}", "\n\n", "".join(out)).strip()
    data = clean.encode("utf-8")
    if len(data) > MAX_ANSWER:
        data = data[:MAX_ANSWER - 6].decode("utf-8", "ignore").encode("utf-8") + b" [...]"
    return data.decode("utf-8")


def loopback_only(url: str) -> str:
    """The AI server must be on this machine: anything else is refused (fail-closed)."""
    u = urllib.parse.urlparse(url)
    if u.scheme != "http" or not u.hostname:
        raise SystemExit(f"ia-ponte: servidor inválido {url!r} (use http://127.0.0.1:PORTA)")
    host = u.hostname
    if host != "localhost":
        try:
            if not ipaddress.ip_address(host).is_loopback:
                raise ValueError
        except ValueError:
            raise SystemExit(f"ia-ponte: o servidor de IA deve ser local (127.0.0.1, ::1 ou localhost), não {host!r}")
    return url.rstrip("/")


class Bridge:
    def __init__(self, server, model, max_tokens, timeout, history):
        self.server, self.model = server, model
        self.max_tokens, self.timeout, self.keep = max_tokens, timeout, history
        self.history = []
        # Never route loopback traffic through an HTTP proxy from the environment.
        self.http = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def ask(self, question: str, new_conversation: bool):
        if new_conversation:
            self.history.clear()
        messages = [{"role": "system", "content": SYSTEM_PROMPT}] + self.history + [{"role": "user", "content": question}]
        body = {"messages": messages, "max_tokens": self.max_tokens, "temperature": 0.7, "stream": False}
        if self.model:
            body["model"] = self.model
        req = urllib.request.Request(self.server + "/v1/chat/completions", data=json.dumps(body).encode(),
                                     headers={"Content-Type": "application/json"})
        try:
            with self.http.open(req, timeout=self.timeout) as r:
                reply = json.loads(r.read(4 << 20).decode("utf-8", "replace"))
            content = reply["choices"][0]["message"]["content"]
            if not isinstance(content, str):
                raise ValueError("conteúdo não é texto")
        except urllib.error.HTTPError as e:
            return "ER", f"servidor de IA respondeu HTTP {e.code}"
        except (urllib.error.URLError, OSError) as e:
            reason = getattr(e, "reason", e)
            return "ER", f"servidor de IA inacessível ({to_console_text(str(reason))[:80]})"
        except (ValueError, KeyError, IndexError, TypeError):
            return "ER", "resposta do servidor de IA em formato inesperado"
        answer = to_console_text(content) or "(resposta vazia)"
        self.history += [{"role": "user", "content": question}, {"role": "assistant", "content": answer}]
        del self.history[:-2 * self.keep]
        return "OK", answer


def connect(spec: str, wait_s: float) -> socket.socket:
    kind, _, where = spec.partition(":")
    deadline = time.time() + wait_s
    while True:
        try:
            if kind == "unix":
                s = socket.socket(socket.AF_UNIX)
                s.connect(where)
            elif kind == "tcp":
                host, _, port = where.rpartition(":")
                loopback_only(f"http://{host}:{port}")       # same rule: local only
                s = socket.create_connection((host.strip("[]"), int(port)))
            else:
                raise SystemExit(f"ia-ponte: --serial deve ser unix:CAMINHO ou tcp:127.0.0.1:PORTA, não {spec!r}")
            return s
        except OSError:
            if time.time() >= deadline:
                raise SystemExit(f"ia-ponte: não consegui conectar à serial {spec} (o QEMU está rodando?)")
            time.sleep(0.2)


def main():
    ap = argparse.ArgumentParser(description="Ponte serial entre o Peregrinus OS e um servidor de IA local.")
    ap.add_argument("--serial", default="unix:/tmp/peregrinus-serial.sock", help="unix:CAMINHO ou tcp:127.0.0.1:PORTA (COM1 do QEMU)")
    ap.add_argument("--server", default="http://127.0.0.1:8080", help="llama-server (somente loopback)")
    ap.add_argument("--model", default="", help="nome do modelo (opcional; o llama-server usa o carregado)")
    ap.add_argument("--max-tokens", type=int, default=512)
    ap.add_argument("--timeout", type=float, default=100.0, help="segundos por resposta (o kernel espera 120)")
    ap.add_argument("--history", type=int, default=6, help="trocas lembradas por conversa")
    ap.add_argument("--wait", type=float, default=30.0, help="segundos tentando conectar à serial")
    ap.add_argument("--no-stdin", action="store_true", help="não encaminhar o teclado deste terminal")
    a = ap.parse_args()
    bridge = Bridge(loopback_only(a.server), a.model, a.max_tokens, a.timeout, max(0, a.history))
    sock = connect(a.serial, a.wait)
    out = sys.stdout
    out.write(f"[ia-ponte] conectada a {a.serial}; IA em {bridge.server}. Ctrl+C encerra.\n"); out.flush()
    use_stdin = not a.no_stdin and sys.stdin is not None and not sys.stdin.closed
    frame, in_frame = bytearray(), False
    console = codecs.getincrementaldecoder("utf-8")("replace")  # characters may span recv() chunks
    while True:
        rlist = [sock] + ([sys.stdin] if use_stdin else [])
        ready, _, _ = select.select(rlist, [], [])
        if use_stdin and sys.stdin in ready:
            line = sys.stdin.readline()
            if not line:
                use_stdin = False
            else:
                sock.sendall(line.rstrip("\n").encode("utf-8") + b"\r")
        if sock not in ready:
            continue
        data = sock.recv(4096)
        if not data:
            out.write("\n[ia-ponte] a serial fechou (VM desligada).\n"); out.flush()
            return 0
        text = bytearray()
        for b in data:
            if not in_frame:
                if b == STX:
                    in_frame, frame = True, bytearray()
                else:
                    text.append(b)
                continue
            if b == ETX:
                in_frame = False
                handle_frame(bytes(frame), bridge, sock, out)
            elif b == STX or len(frame) >= MAX_FRAME:
                in_frame = b == STX                     # broken frame: dropped
                frame = bytearray()
            else:
                frame.append(b)
        if text:
            out.write(console.decode(bytes(text).replace(b"\r", b""))); out.flush()


def handle_frame(frame: bytes, bridge: Bridge, sock: socket.socket, out):
    header, sep, body = frame.partition(b"\n")
    m = re.fullmatch(rb"PGQ1 (\d{1,10}) ([NC])", header)
    if not sep or not m or int(m.group(1)) > 0xFFFFFFFF:
        out.write("\n[ia-ponte] quadro inválido ignorado\n"); out.flush()
        return
    seq, new = int(m.group(1)), m.group(2) == b"N"
    question = body.decode("utf-8", "replace").strip()
    out.write(f"\n[ia-ponte] pergunta #{seq}{' (nova conversa)' if new else ''}: {question}\n"); out.flush()
    t0 = time.time()
    kind, answer = bridge.ask(question, new) if question else ("ER", "pergunta vazia")
    out.write(f"[ia-ponte] resposta #{seq} ({kind}, {time.time() - t0:.1f} s)\n"); out.flush()
    sock.sendall(b"\x02PGA1 %d %s\n" % (seq, kind.encode()) + answer.encode("utf-8") + b"\x03")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
