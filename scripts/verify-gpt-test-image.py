#!/usr/bin/env python3
import binascii, os, struct, sys, re
from pathlib import Path
p=sys.argv[1] if len(sys.argv)>1 else 'build/peregrinus-testdisk.img'
SECTOR=512
PROFILE=(Path(__file__).resolve().parents[1]/'include/peregrinus/release_profile.h').read_text()
def profile_num(name):
    m=re.search(r'^#define\s+'+name+r'\s+(\d+)',PROFILE,re.M)
    if not m: raise RuntimeError(name)
    return int(m.group(1))
CURRENT_GEN=profile_num('PEREGRINUS_RELEASE_CURRENT_GENERATION')
LKG_GEN=profile_num('PEREGRINUS_RELEASE_LKG_GENERATION')
CURRENT_EPOCH=profile_num('PEREGRINUS_RELEASE_CURRENT_EPOCH')
LKG_EPOCH=profile_num('PEREGRINUS_RELEASE_LKG_EPOCH')

def crc32(data): return binascii.crc32(data)&0xffffffff

def check_header(h, expected_current, expected_backup):
    assert h[:8]==b'EFI PART'
    size=struct.unpack_from('<I',h,12)[0]
    want=struct.unpack_from('<I',h,16)[0]
    t=bytearray(h[:size]); struct.pack_into('<I',t,16,0)
    assert want==crc32(t)
    current,backup=struct.unpack_from('<QQ',h,24)
    assert current==expected_current and backup==expected_backup
    count,entry_size,entries_crc=struct.unpack_from('<III',h,80)
    return count,entry_size,entries_crc,struct.unpack_from('<Q',h,72)[0],h[56:72]

def check_recovery_journal(j):
    assert j[:8]==b'PGRJNL02'
    version,hsize,sequence=struct.unpack_from('<IIQ',j,8)
    assert version==2 and hsize==112 and sequence>=1
    assert j[24:40]==pguid
    current_gen,lkg_gen,current_epoch,lkg_epoch,current_attempts,lkg_attempts,max_attempts=struct.unpack_from('<QQQQIII',j,56)
    assert (current_gen,lkg_gen,current_epoch,lkg_epoch)==(CURRENT_GEN,LKG_GEN,CURRENT_EPOCH,LKG_EPOCH)
    assert current_attempts<max_attempts and lkg_attempts<max_attempts and max_attempts==3
    assert j[100] in (0,1) and (j[101]&1)
    want=struct.unpack_from('<I',j,108)[0]
    t=bytearray(j[:hsize]);struct.pack_into('<I',t,108,0)
    assert crc32(t)==want
    return j[24:56],sequence

def check_anchor(a):
    assert a[:16]==b'PEREGRINUS-RCV1\x00'
    version,hsize,generation,flags,want=struct.unpack_from('<IIQII',a,16)
    assert version==1 and hsize==192 and generation>=1 and (flags&1)
    t=bytearray(a[:hsize]); struct.pack_into('<I',t,36,0)
    assert crc32(t)==want
    sf,sl,rf,rl,df,dl=struct.unpack_from('<QQQQQQ',a,104)
    assert (sf,sl,rf,rl,df)==(2048,32767,32768,49151,49152)
    assert dl>=df
    return a[40:104], generation, flags

size=os.path.getsize(p); last=size//SECTOR-1
with open(p,'rb') as f:
    mbr=f.read(SECTOR); ph=f.read(SECTOR)
    f.seek(last*SECTOR); bh=f.read(SECTOR)
assert mbr[510:512]==b'\x55\xaa'
pc,ps,pcrc,pelba,pguid=check_header(ph,1,last)
bc,bs,bcrc,belba,bguid=check_header(bh,last,1)
assert (pc,ps,pcrc,pguid)==(bc,bs,bcrc,bguid)
with open(p,'rb') as f:
    f.seek(pelba*SECTOR); pe=f.read(pc*ps)
    f.seek(belba*SECTOR); be=f.read(bc*bs)
    f.seek(32768*SECTOR); ra=f.read(SECTOR)
    f.seek(32769*SECTOR); legacy_a=f.read(SECTOR)
    f.seek(32770*SECTOR); rja=f.read(SECTOR)
    f.seek(49149*SECTOR); rjb=f.read(SECTOR)
    f.seek(49150*SECTOR); legacy_b=f.read(SECTOR)
    f.seek(49151*SECTOR); rb=f.read(SECTOR)
assert len(pe)==pc*ps and len(be)==bc*bs
assert crc32(pe)==pcrc and crc32(be)==bcrc and pe==be
names=[]
for i in range(3):
    e=pe[i*ps:(i+1)*ps]
    names.append(e[56:128].decode('utf-16le').split('\0',1)[0])
assert names==['IA_SYSTEM','IA_RECOVERY','IA_DATA'],names
assert belba + ((bc*bs + SECTOR-1)//SECTOR) == last
ida,ga,fa=check_anchor(ra); idb,gb,fb=check_anchor(rb)
assert ida==idb and ga==gb and fa==fb
assert ida[0:16]==pguid
assert legacy_a==bytes(SECTOR) and legacy_b==bytes(SECTOR)
rida,rsa=check_recovery_journal(rja);ridb,rsb=check_recovery_journal(rjb)
assert rida==ridb and rsa==rsb and rida[0:16]==pguid
print('PASS: redundant GPT + protected volumes + coherent A/B anchors + reserved legacy sectors + recovery journal')
