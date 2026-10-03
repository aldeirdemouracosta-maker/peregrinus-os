#!/usr/bin/env python3
import hashlib, os, shutil, struct, subprocess, sys, tempfile

if len(sys.argv) < 2:
    raise SystemExit("usage: seal-kernel.py <elf> [label]")
elf=sys.argv[1]
label=(sys.argv[2] if len(sys.argv)>2 else "UNLABELED")[:31]
objcopy=shutil.which("llvm-objcopy") or shutil.which("objcopy")
if not objcopy:
    raise SystemExit("objcopy/llvm-objcopy not found")
with tempfile.TemporaryDirectory() as td:
    text_path=os.path.join(td,"text.bin")
    manifest_path=os.path.join(td,"manifest.bin")
    subprocess.check_call([objcopy,"--dump-section",f".text={text_path}",elf])
    text=open(text_path,"rb").read()
    digest=hashlib.sha256(text).digest()
    magic=b"PEREGRINUS-GUARD"
    lab=label.encode("ascii","strict")+b"\0"
    lab=lab.ljust(32,b"\0")[:32]
    manifest=magic+struct.pack("<IIQ",1,96,len(text))+digest+lab
    assert len(manifest)==96
    open(manifest_path,"wb").write(manifest)
    subprocess.check_call([objcopy,"--update-section",f".peregrinus_integrity={manifest_path}",elf])
    print(f"sealed {elf}: text={len(text)} sha256={digest.hex()} label={label}")
