#!/usr/bin/env python3
"""After a recovery-commit boot, the newest valid journal copy must record BOOT_SUCCESS
for CURRENT with the attempt counter reset — i.e. the kernel's write really reached the disk."""
import binascii, struct, sys

SECTOR = 512
REC_FIRST, REC_LAST = 32768, 49151  # IA_RECOVERY in scripts/create-gpt-test-image.py

def record(img, lba):
    img.seek(lba * SECTOR)
    h = img.read(112)
    if h[0:8] != b'PGRJNL02':
        return None
    crc = struct.unpack_from('<I', h, 108)[0]
    if binascii.crc32(h[:108] + b'\0\0\0\0') & 0xffffffff != crc:
        return None
    seq = struct.unpack_from('<Q', h, 16)[0]
    cur_att, lkg_att = struct.unpack_from('<II', h, 88)
    return {'seq': seq, 'current_attempts': cur_att, 'lkg_attempts': lkg_att, 'last_slot': h[100], 'flags': h[101]}

with open(sys.argv[1], 'rb') as img:
    copies = [r for r in (record(img, REC_FIRST + 2), record(img, REC_LAST - 2)) if r]
if not copies:
    print('FAIL: no valid journal copy on disk'); sys.exit(1)
newest = max(copies, key=lambda r: r['seq'])
if not (newest['flags'] & 0x2) or newest['current_attempts'] != 0 or newest['last_slot'] != 0:
    print('FAIL: newest journal copy does not record a CURRENT boot success:', newest); sys.exit(1)
print('journal: seq=%d flags=0x%x current_attempts=%d' % (newest['seq'], newest['flags'], newest['current_attempts']))
