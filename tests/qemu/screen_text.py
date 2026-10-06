#!/usr/bin/env python3
"""Decode the text on a QEMU screendump (PPM) rendered by the Peregrinus text console.

The console draws glyphs from kernel/console/font8x8.hpp with exact colours, so every cell can
be matched bit-for-bit against the font. Usage: screen_text.py <dump.ppm> [expected text...]
Prints the decoded screen; exits 1 if any expected text is missing."""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
FG = (0xD8, 0xDE, 0xE6)

def load_font():
    src = (ROOT / 'kernel/console/font8x8.hpp').read_text()
    rows = re.findall(r'\{((?:\s*0x[0-9A-Fa-f]{2},?){8})\}', src)
    font = {}
    for idx, r in enumerate(rows):
        bits = tuple(int(v, 16) for v in r.replace(' ', '').split(',') if v)
        code = idx if idx < 128 else 0xA0 + (idx - 128)  # basic table, then Latin-1 supplement
        if 0x20 <= code < 0x7F or 0xA1 <= code <= 0xFF:
            font.setdefault(bits, chr(code))
    return font

def load_ppm(path):
    data = pathlib.Path(path).read_bytes()
    tokens, pos = [], 0
    while len(tokens) < 4:
        while data[pos:pos+1].isspace(): pos += 1
        if data[pos:pos+1] == b'#':
            pos = data.index(b'\n', pos) + 1; continue
        start = pos
        while not data[pos:pos+1].isspace(): pos += 1
        tokens.append(data[start:pos])
    assert tokens[0] == b'P6' and int(tokens[3]) == 255
    w, h = int(tokens[1]), int(tokens[2])
    return w, h, data[pos+1:]

def decode(path):
    w, h, px = load_ppm(path)
    scale = 2 if (w >= 1024 and h >= 600) else 1
    while scale < 4 and (w // (8 * scale) > 160 or h // (10 * scale) > 90):
        scale += 1
    cw, chh = 8 * scale, 10 * scale
    font = load_font()
    lines = []
    for r in range(h // chh):
        out = []
        for c in range(w // cw):
            bits = []
            for gy in range(8):
                b = 0
                for gx in range(8):
                    x, y = c * cw + gx * scale, r * chh + gy * scale
                    i = (y * w + x) * 3
                    if tuple(px[i:i+3]) == FG: b |= 1 << gx
                bits.append(b)
            out.append(font.get(tuple(bits), '#'))
        lines.append(''.join(out).rstrip())
    return lines

if __name__ == '__main__':
    lines = decode(sys.argv[1])
    print('\n'.join(l for l in lines if l))
    screen = '\n'.join(lines)
    missing = [t for t in sys.argv[2:] if t not in screen]
    if missing:
        print('FAIL: not on screen:', missing); sys.exit(1)
