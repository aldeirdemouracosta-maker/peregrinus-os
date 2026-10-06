#!/usr/bin/env python3
"""Fake llama-server for the AI bridge tests (no GPU, no model): answers
/v1/chat/completions with deterministic text. Prints the port it listens on (127.0.0.1)."""
import json, sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        if self.path != "/v1/chat/completions":
            self.send_error(404); return
        req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
        msgs = req["messages"]
        q = msgs[-1]["content"]
        if "lixo" in q:
            content = ("<think>raciocinio escondido</think>Linha \x1b[2Jlimpa\x07 “aspas” — "
                       "fim… emoji \U0001F642 C1\u009b ok\t tab\r\n\n\n\nação")
        elif "longo" in q:
            content = "ç" * 3000
        else:
            content = f"Resposta de teste: {q} (historico={len(msgs)})"
        body = json.dumps({"choices": [{"message": {"role": "assistant", "content": content}}]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


srv = HTTPServer(("127.0.0.1", 0), H)
print(srv.server_address[1], flush=True)
srv.serve_forever()
