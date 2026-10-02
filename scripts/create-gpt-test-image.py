#!/usr/bin/env python3
import argparse, binascii, os, struct, uuid, re
from pathlib import Path

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

def guid_le(u: uuid.UUID) -> bytes:
    b=u.bytes
    return struct.pack('<IHH', int.from_bytes(b[0:4],'big'), int.from_bytes(b[4:6],'big'), int.from_bytes(b[6:8],'big')) + b[8:]

def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xffffffff

def put_entry(entries, idx, type_guid, unique_guid, first, last, name):
    off=idx*128
    entries[off:off+16]=guid_le(type_guid)
    entries[off+16:off+32]=guid_le(unique_guid)
    struct.pack_into('<QQQ', entries, off+32, first, last, 0)
    raw=name.encode('utf-16le')[:72]
    entries[off+56:off+56+len(raw)]=raw

def main():
    ap=argparse.ArgumentParser(description='Create disposable GPT image for Peregrinus OS Muro 1.0.1 Hardened recovery test')
    ap.add_argument('path', nargs='?', default='build/peregrinus-jo-1.0-testdisk.img')
    ap.add_argument('--mib', type=int, default=64)
    a=ap.parse_args()
    size=a.mib*1024*1024
    total=size//SECTOR
    if total < 65536: raise SystemExit('image too small for Muro 1.0.1 layout')
    os.makedirs(os.path.dirname(a.path) or '.', exist_ok=True)
    with open(a.path,'wb') as f: f.truncate(size)

    mbr=bytearray(512)
    mbr[446+4]=0xEE
    struct.pack_into('<I',mbr,446+8,1)
    struct.pack_into('<I',mbr,446+12,min(total-1,0xffffffff))
    mbr[510:512]=b'\x55\xaa'

    entries=bytearray(128*128)
    type_guid=uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4')
    first_usable=34
    last_usable=total-34
    put_entry(entries,0,type_guid,uuid.UUID('11111111-1111-4111-8111-111111111111'),2048,32767,'IA_SYSTEM')
    put_entry(entries,1,type_guid,uuid.UUID('22222222-2222-4222-8222-222222222222'),32768,49151,'IA_RECOVERY')
    put_entry(entries,2,type_guid,uuid.UUID('33333333-3333-4333-8333-333333333333'),49152,last_usable,'IA_DATA')
    ecrc=crc32(entries)
    disk_guid=uuid.UUID('01234567-89ab-cdef-0123-456789abcdef')

    def header(current,backup,entries_lba):
        h=bytearray(512)
        h[0:8]=b'EFI PART'
        struct.pack_into('<I',h,8,0x00010000)
        struct.pack_into('<I',h,12,92)
        struct.pack_into('<I',h,16,0)
        struct.pack_into('<QQQQ',h,24,current,backup,first_usable,last_usable)
        h[56:72]=guid_le(disk_guid)
        struct.pack_into('<QIII',h,72,entries_lba,128,128,ecrc)
        struct.pack_into('<I',h,16,crc32(h[:92]))
        return h

    ph=header(1,total-1,2)
    backup_entries_lba=total-33
    bh=header(total-1,1,backup_entries_lba)
    def recovery_anchor(generation=1, flags=0x3):
        h=bytearray(512)
        h[0:16]=b'PEREGRINUS-RCV1\x00'
        struct.pack_into('<IIQII',h,16,1,192,generation,flags,0)
        h[40:56]=guid_le(disk_guid)
        h[56:72]=guid_le(uuid.UUID('22222222-2222-4222-8222-222222222222'))
        h[72:88]=guid_le(uuid.UUID('11111111-1111-4111-8111-111111111111'))
        h[88:104]=guid_le(uuid.UUID('33333333-3333-4333-8333-333333333333'))
        struct.pack_into('<QQQQQQ',h,104,2048,32767,32768,49151,49152,last_usable)
        label=b'MURO-101-RCV' 
        h[152:152+len(label)]=label
        struct.pack_into('<I',h,36,crc32(h[:192]))
        return h

    def recovery_journal(sequence=1,current_attempts=0,lkg_attempts=0,current_generation=CURRENT_GEN,lkg_generation=LKG_GEN,current_epoch=CURRENT_EPOCH,lkg_epoch=LKG_EPOCH):
        h=bytearray(512)
        h[0:8]=b'PGRJNL02'
        struct.pack_into('<IIQ',h,8,2,112,sequence)
        h[24:40]=guid_le(disk_guid)
        h[40:56]=guid_le(uuid.UUID('22222222-2222-4222-8222-222222222222'))
        struct.pack_into('<QQQQIII',h,56,current_generation,lkg_generation,current_epoch,lkg_epoch,current_attempts,lkg_attempts,3)
        h[100]=0
        h[101]=1
        struct.pack_into('<I',h,108,0)
        struct.pack_into('<I',h,108,crc32(h[:112]))
        return h

    ra=recovery_anchor()
    rj=recovery_journal()
    with open(a.path,'r+b') as f:
        f.seek(0); f.write(mbr)
        f.seek(SECTOR); f.write(ph)
        f.seek(2*SECTOR); f.write(entries)
        f.seek(32768*SECTOR); f.write(ra)
        f.seek(32769*SECTOR); f.write(bytes(SECTOR))  # reserved legacy Boot Control sector
        f.seek(32770*SECTOR); f.write(rj)
        f.seek(49149*SECTOR); f.write(rj)
        f.seek(49150*SECTOR); f.write(bytes(SECTOR))  # reserved legacy Boot Control sector
        f.seek(49151*SECTOR); f.write(ra)
        f.seek(backup_entries_lba*SECTOR); f.write(entries)
        f.seek((total-1)*SECTOR); f.write(bh)
    print(f'created {a.path}: {a.mib} MiB, redundant GPT + A/B recovery anchors + reserved legacy sectors + A/B recovery journal, partitions=IA_SYSTEM/IA_RECOVERY/IA_DATA, primary CRC=0x{struct.unpack_from("<I",ph,16)[0]:08x}')

if __name__=='__main__': main()
