#!/usr/bin/env python3
import argparse, struct, zlib, pathlib, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def profile(key):
    return int(subprocess.check_output([str(ROOT/'scripts/release-profile.py'),key],text=True).strip())
ap=argparse.ArgumentParser(description='Create Peregrinus one-shot preboot request payload (does not write NVRAM).')
ap.add_argument('--choice',choices=['current','lkg'],required=True)
ap.add_argument('--epoch',type=int)
ap.add_argument('--generation',type=int)
ap.add_argument('--output',required=True)
a=ap.parse_args(); choice=0 if a.choice=='current' else 1
def_epoch=profile('current_epoch' if choice==0 else 'lkg_epoch')
def_gen=profile('current_generation' if choice==0 else 'lkg_generation')
epoch=a.epoch if a.epoch is not None else def_epoch
generation=a.generation if a.generation is not None else def_gen
magic=b'PGRREQ01'
base=struct.pack('<8sIB3xQQI',magic,1,choice,epoch,generation,0)
crc=zlib.crc32(base)&0xffffffff
payload=struct.pack('<8sIB3xQQI',magic,1,choice,epoch,generation,crc)
pathlib.Path(a.output).write_bytes(payload)
print(f'{a.output}: {len(payload)} bytes choice={a.choice} epoch={epoch} generation={generation} crc32={crc:08x}')
