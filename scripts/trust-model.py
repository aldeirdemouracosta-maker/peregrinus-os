#!/usr/bin/env python3
"""Add a local-LLM model to the kernel allowlist (include/peregrinus/model_allowlist.h).

The kernel only loads a model whose model.bin AND tokenizer.bin SHA-256 digests match one
entry (Purgatorio allowlist, fail-closed). Rebuild the llm-local profile afterwards.
Usage: trust-model.py <name> <model.bin> <tokenizer.bin>"""
import hashlib, re, sys
from pathlib import Path

HDR = Path(__file__).resolve().parents[1] / 'include/peregrinus/model_allowlist.h'
name, model, tok = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
if not re.fullmatch(r'[A-Za-z0-9._-]{1,40}', name):
    raise SystemExit('name: 1-40 chars of [A-Za-z0-9._-]')
def digest(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''): h.update(chunk)
    return h.digest()
def c_bytes(d): return '{' + ', '.join('0x%02x' % b for b in d) + '}'
entry = '    {"%s", %s,\n     %s},\n' % (name, c_bytes(digest(model)), c_bytes(digest(tok)))
text = HDR.read_text()
if c_bytes(digest(model)) in text and c_bytes(digest(tok)) in text:
    print('already trusted:', name); sys.exit(0)
marker = '    // END OF ALLOWLIST\n'
if marker not in text: raise SystemExit('allowlist marker missing')
HDR.write_text(text.replace(marker, entry + marker))
print('trusted:', name, digest(model).hex()[:16], digest(tok).hex()[:16])
