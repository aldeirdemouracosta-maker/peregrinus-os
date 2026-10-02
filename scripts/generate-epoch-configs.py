#!/usr/bin/env python3
import argparse, hashlib, pathlib

def b2(path):
    h=hashlib.blake2b()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()

ap=argparse.ArgumentParser(description='Generate staged/committed Limine configs for Peregrinus')
ap.add_argument('--current',required=True); ap.add_argument('--lkg',required=True)
ap.add_argument('--staged-output',required=True); ap.add_argument('--committed-output',required=True)
ap.add_argument('--current-generation',type=int,required=True); ap.add_argument('--lkg-generation',type=int,required=True)
ap.add_argument('--current-epoch',type=int,required=True); ap.add_argument('--lkg-epoch',type=int,required=True)
a=ap.parse_args(); cur=pathlib.Path(a.current); lkg=pathlib.Path(a.lkg)
common='''timeout: 2\ndefault_entry: Peregrinus/CURRENT\nremember_last_entry: no\nhash_mismatch_panic: yes\nmeasured_boot: yes\n\n/+Peregrinus\n'''
staged=f'''# Peregrinus OS Muro 1.0.1 STAGED\n# current_generation: {a.current_generation}\n# lkg_generation: {a.lkg_generation}\n# current_security_epoch: {a.current_epoch}\n# lkg_security_epoch: {a.lkg_epoch}\n{common}//CURRENT\n    comment: generation {a.current_generation}, security epoch {a.current_epoch}\n    protocol: limine\n    path: boot():/boot/peregrinus-current.elf#{b2(cur)}\n\n//LKG\n    comment: fallback generation {a.lkg_generation}, security epoch {a.lkg_epoch}\n    protocol: limine\n    path: boot():/boot/peregrinus-lkg.elf#{b2(lkg)}\n'''
committed=f'''# Peregrinus OS Muro 1.0.1 COMMITTED\n# current_generation: {a.current_generation}\n# lkg_generation: {a.lkg_generation}\n# current_security_epoch: {a.current_epoch}\n# lkg_security_epoch: {a.lkg_epoch}\n{common}//CURRENT\n    comment: committed generation {a.current_generation}, security epoch {a.current_epoch}\n    protocol: limine\n    path: boot():/boot/peregrinus-current.elf#{b2(cur)}\n'''
if a.lkg_epoch==a.current_epoch:
    committed += f'''\n//LKG\n    comment: authenticated fallback generation {a.lkg_generation}, security epoch {a.lkg_epoch}\n    protocol: limine\n    path: boot():/boot/peregrinus-lkg.elf#{b2(lkg)}\n'''
else:
    committed += '# old-epoch LKG intentionally omitted after commit\n'
for out,text in ((a.staged_output,staged),(a.committed_output,committed)):
    p=pathlib.Path(out); p.parent.mkdir(parents=True,exist_ok=True); p.write_text(text); print(p)
