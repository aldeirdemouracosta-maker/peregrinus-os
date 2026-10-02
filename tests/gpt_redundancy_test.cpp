#include "../kernel/storage/gpt.hpp"
#include <stdio.h>
using namespace peregrinus::gpt;
static HeaderInfo h(uint64_t cur,uint64_t bak,uint64_t ent,uint32_t crc){ HeaderInfo x{};x.valid_signature=x.valid_header_size=x.valid_crc=true;x.header_size=92;x.current_lba=cur;x.backup_lba=bak;x.first_usable_lba=34;x.last_usable_lba=966;x.entries_lba=ent;x.entry_count=1;x.entry_size=128;x.entries_crc32=crc;x.disk_guid.bytes[0]=0x42;return x; }
static TableInfo t(){ TableInfo x{};x.header_ok=x.entries_crc_ok=true;return x; }
int main(){
    auto ph=h(1,999,2,0x1234), bh=h(999,1,998,0x1234); auto pt=t(),bt=t();
    auto pv=validate_copy(ph,pt,CopyKind::primary,999,512),bv=validate_copy(bh,bt,CopyKind::backup,999,512);
    auto healthy=assess_redundancy(ph,pt,pv,bh,bt,bv,true); if(healthy.selected!=Selection::primary||healthy.degraded||healthy.split_brain)return 1;
    CopyValidation bad{}; auto ponly=assess_redundancy(ph,pt,pv,bh,bt,bad,false); if(ponly.selected!=Selection::primary||!ponly.degraded)return 2;
    auto bonly=assess_redundancy(ph,pt,bad,bh,bt,bv,false); if(bonly.selected!=Selection::backup||!bonly.degraded)return 3;
    bh.disk_guid.bytes[0]=0x99; auto split=assess_redundancy(ph,pt,pv,bh,bt,bv,true); if(split.selected!=Selection::none||!split.split_brain)return 4;
    puts("PASS: GPT redundancy selection host self-test."); return 0;
}
