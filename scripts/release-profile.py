#!/usr/bin/env python3
import re, sys
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'include/peregrinus/release_profile.h'
t=p.read_text()
keys={
 'name':'PEREGRINUS_RELEASE_NAME','tag':'PEREGRINUS_RELEASE_TAG',
 'current_generation':'PEREGRINUS_RELEASE_CURRENT_GENERATION',
 'lkg_generation':'PEREGRINUS_RELEASE_LKG_GENERATION',
 'current_epoch':'PEREGRINUS_RELEASE_CURRENT_EPOCH',
 'lkg_epoch':'PEREGRINUS_RELEASE_LKG_EPOCH',
 'min_epoch':'PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH'}
if len(sys.argv)!=2 or sys.argv[1] not in keys:
    raise SystemExit('usage: release-profile.py '+ '|'.join(keys))
m=re.search(r'^#define\s+'+re.escape(keys[sys.argv[1]])+r'\s+(.+?)\s*$',t,re.M)
if not m: raise SystemExit('missing '+keys[sys.argv[1]])
v=m.group(1).strip()
if v.startswith('"') and v.endswith('"'): print(v[1:-1])
else: print(re.sub(r'(?i)(ull|ul|ll|u|l)$','',v))
