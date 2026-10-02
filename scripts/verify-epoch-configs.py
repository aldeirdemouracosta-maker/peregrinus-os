#!/usr/bin/env python3
import argparse, hashlib, pathlib, re
ap=argparse.ArgumentParser(); ap.add_argument('--staged',required=True); ap.add_argument('--committed',required=True); ap.add_argument('--root',required=True); ap.add_argument('--committed-lkg',action='store_true'); a=ap.parse_args()
root=pathlib.Path(a.root)
def verify(path, expected):
    text=pathlib.Path(path).read_text(); paths=re.findall(r'^\s*path:\s*boot\(\):(/[^#\s]+)#([0-9a-fA-F]{128})\s*$',text,re.M)
    if len(paths)!=expected: raise SystemExit(f'FAIL: {path} expected {expected} pinned paths, got {len(paths)}')
    for rel,want in paths:
        p=root/rel.lstrip('/')
        if not p.is_file(): raise SystemExit(f'FAIL: missing {p}')
        got=hashlib.blake2b(p.read_bytes()).hexdigest()
        if got.lower()!=want.lower(): raise SystemExit(f'FAIL: hash mismatch {p}')
    return text
st=verify(a.staged,2); cm=verify(a.committed,2 if a.committed_lkg else 1)
if '//LKG' not in st: raise SystemExit('FAIL: staged config lacks LKG')
if a.committed_lkg:
    if '//LKG' not in cm or 'peregrinus-lkg.elf#' not in cm: raise SystemExit('FAIL: committed config lacks same-epoch LKG')
else:
    if '//LKG' in cm or 'peregrinus-lkg.elf#' in cm: raise SystemExit('FAIL: committed config exposes old-epoch LKG')
for t in (st,cm):
    for x in ('default_entry: Peregrinus/CURRENT','remember_last_entry: no','hash_mismatch_panic: yes','measured_boot: yes'):
        if x not in t: raise SystemExit(f'FAIL: missing {x}')
print('PASS: staged/committed pinned configs verified')
