#include "gpt.hpp"

namespace peregrinus::gpt {
namespace {
static uint32_t rd32(const uint8_t* p){ return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
static uint64_t rd64(const uint8_t* p){ return uint64_t(rd32(p))|(uint64_t(rd32(p+4))<<32); }
static void wr32(uint8_t* p,uint32_t v){ for(int i=0;i<4;++i)p[i]=uint8_t(v>>(8*i)); }
static void wr64(uint8_t* p,uint64_t v){ for(int i=0;i<8;++i)p[i]=uint8_t(v>>(8*i)); }
static uint32_t crc32(const uint8_t* p,size_t n){ uint32_t c=0xFFFFFFFFu; for(size_t i=0;i<n;++i){ c^=p[i]; for(int b=0;b<8;++b)c=(c>>1)^(0xEDB88320u & (0u-(c&1u))); } return ~c; }
static bool all_zero(const uint8_t* p,size_t n){ for(size_t i=0;i<n;++i) if(p[i]) return false; return true; }
static bool equals(const char* a,const char* b){ while(*a&&*b){ if(*a++!=*b++) return false; } return *a==0&&*b==0; }
static void copy_guid(Guid& out,const uint8_t* src){ for(unsigned i=0;i<16;++i) out.bytes[i]=src[i]; }
static void decode_name(const uint8_t* src,char out[37]){
    size_t j=0;
    for(size_t i=0;i<36 && j<36;++i){
        uint16_t ch=uint16_t(src[i*2])|(uint16_t(src[i*2+1])<<8);
        if(ch==0) break;
        out[j++]=(ch>=32 && ch<127)?char(ch):'?';
    }
    out[j]=0;
}
static PartitionRole role_for(const char* n){
    if(equals(n,"IA_SYSTEM")) return PartitionRole::system;
    if(equals(n,"IA_RECOVERY")) return PartitionRole::recovery;
    if(equals(n,"IA_DATA")) return PartitionRole::data;
    return PartitionRole::unknown;
}
static char hx(uint8_t n){ return n<10?char('0'+n):char('a'+(n-10)); }
static void hex_byte(uint8_t v,char* out){ out[0]=hx(v>>4); out[1]=hx(v&0x0F); }
static bool add_ok(uint64_t a,uint64_t b,uint64_t& out){ out=a+b; return out>=a; }
static bool coherent_headers(const HeaderInfo& p,const HeaderInfo& b){
    return p.current_lba==b.backup_lba && p.backup_lba==b.current_lba &&
           p.first_usable_lba==b.first_usable_lba && p.last_usable_lba==b.last_usable_lba &&
           guid_equal(p.disk_guid,b.disk_guid) && p.entry_count==b.entry_count && p.entry_size==b.entry_size &&
           p.entries_crc32==b.entries_crc32;
}
}

HeaderInfo parse_header(const uint8_t* s,size_t bytes){
    HeaderInfo o{}; if(!s || bytes<92) return o;
    o.valid_signature=s[0]=='E'&&s[1]=='F'&&s[2]=='I'&&s[3]==' '&&s[4]=='P'&&s[5]=='A'&&s[6]=='R'&&s[7]=='T';
    o.revision=rd32(s+8); o.header_size=rd32(s+12); o.valid_header_size=o.header_size>=92&&o.header_size<=bytes;
    o.current_lba=rd64(s+24); o.backup_lba=rd64(s+32); o.first_usable_lba=rd64(s+40); o.last_usable_lba=rd64(s+48);
    copy_guid(o.disk_guid,s+56);
    o.entries_lba=rd64(s+72); o.entry_count=rd32(s+80); o.entry_size=rd32(s+84); o.entries_crc32=rd32(s+88);
    if(o.valid_signature&&o.valid_header_size){
        uint8_t tmp[512]{};
        if(o.header_size<=sizeof(tmp)){ for(uint32_t i=0;i<o.header_size;++i) tmp[i]=s[i]; uint32_t expect=rd32(tmp+16); wr32(tmp+16,0); o.valid_crc=crc32(tmp,o.header_size)==expect; }
    }
    return o;
}

TableInfo parse_entries(const HeaderInfo& h,const uint8_t* data,size_t bytes){
    TableInfo t{}; t.header_ok=h.valid_signature&&h.valid_header_size&&h.valid_crc; t.declared_entries=h.entry_count;
    if(!t.header_ok || !data || h.entry_count==0 || h.entry_size<128 || h.entry_size>4096 || (h.entry_size&7u)!=0) return t;
    const uint64_t total=uint64_t(h.entry_count)*uint64_t(h.entry_size);
    if(total>bytes || total>size_t(-1)) return t;
    t.entries_crc_ok=crc32(data,size_t(total))==h.entries_crc32;
    const uint32_t limit=(h.entry_count<16)?h.entry_count:16;
    for(uint32_t i=0;i<limit;++i){
        const uint8_t* e=data+size_t(i)*h.entry_size; ++t.parsed_entries; if(all_zero(e,16)) continue;
        auto& p=t.partitions[t.used_entries]; p.used=true; copy_guid(p.type_guid,e); copy_guid(p.unique_guid,e+16);
        p.first_lba=rd64(e+32); p.last_lba=rd64(e+40); p.attributes=rd64(e+48); decode_name(e+56,p.name); p.role=role_for(p.name);
        ++t.used_entries; if(t.used_entries==16) break;
    }
    return t;
}

CopyValidation validate_copy(const HeaderInfo& h,const TableInfo& t,CopyKind kind,uint64_t last_lba,uint32_t sector_bytes){
    CopyValidation v{};
    v.header_ok=h.valid_signature&&h.valid_header_size&&h.valid_crc;
    if(!v.header_ok || sector_bytes<512 || h.header_size>sector_bytes || h.entry_count==0 || h.entry_size<128 || (h.entry_size&7u)!=0) return v;
    const uint64_t total=uint64_t(h.entry_count)*uint64_t(h.entry_size); if(h.entry_count && total/h.entry_count!=h.entry_size) return v;
    const uint64_t sectors=(total+sector_bytes-1u)/sector_bytes; if(sectors==0) return v;
    uint64_t entries_end=0; if(!add_ok(h.entries_lba,sectors,entries_end)) return v;
    const bool common=h.first_usable_lba<=h.last_usable_lba && h.last_usable_lba<last_lba && h.backup_lba<=last_lba;
    bool layout=false;
    if(kind==CopyKind::primary){
        layout=common && h.current_lba==1 && h.backup_lba==last_lba && h.entries_lba>h.current_lba && entries_end<=h.first_usable_lba;
    }else{
        layout=common && h.current_lba==last_lba && h.backup_lba==1 && h.entries_lba>h.last_usable_lba && entries_end==h.current_lba;
    }
    v.layout_ok=layout; v.entries_ok=t.header_ok&&t.entries_crc_ok; v.valid=v.header_ok&&v.layout_ok&&v.entries_ok; return v;
}

RedundancyInfo assess_redundancy(const HeaderInfo& ph,const TableInfo&,const CopyValidation& p,
                                 const HeaderInfo& bh,const TableInfo&,const CopyValidation& b,bool arrays_equal){
    RedundancyInfo r{}; r.primary_valid=p.valid; r.backup_valid=b.valid;
    r.headers_coherent=p.valid&&b.valid&&coherent_headers(ph,bh); r.tables_match=p.valid&&b.valid&&arrays_equal&&ph.entries_crc32==bh.entries_crc32;
    if(p.valid&&b.valid){
        if(r.headers_coherent&&r.tables_match){ r.selected=Selection::primary; }
        else { r.split_brain=true; r.selected=Selection::none; }
    }else if(p.valid){ r.selected=Selection::primary; r.degraded=true; }
    else if(b.valid){ r.selected=Selection::backup; r.degraded=true; }
    else { r.selected=Selection::none; r.degraded=true; }
    return r;
}

const char* role_name(PartitionRole r){ switch(r){ case PartitionRole::system:return "SYSTEM"; case PartitionRole::recovery:return "RECOVERY"; case PartitionRole::data:return "DATA"; default:return "UNKNOWN"; } }
const char* selection_name(Selection s){ switch(s){ case Selection::primary:return "PRIMARY"; case Selection::backup:return "BACKUP"; default:return "NONE"; } }
bool guid_equal(const Guid& a,const Guid& b){ for(unsigned i=0;i<16;++i) if(a.bytes[i]!=b.bytes[i]) return false; return true; }
void guid_text(const Guid& g,char out[37]){ const unsigned order[16]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15}; unsigned j=0; for(unsigned i=0;i<16;++i){ if(i==4||i==6||i==8||i==10) out[j++]='-'; const uint8_t b=g.bytes[order[i]]; hex_byte(b,out+j); j+=2; } out[j]=0; }

bool self_test(){
    uint8_t psec[512]{},bsec[512]{}; uint8_t entries[128]{}; const char sig[8]={'E','F','I',' ','P','A','R','T'};
    entries[0]=0xAA; entries[16]=0x11; wr64(entries+32,34); wr64(entries+40,966); const char* nm="IA_SYSTEM"; for(size_t i=0;nm[i];++i){ entries[56+i*2]=uint8_t(nm[i]); entries[57+i*2]=0; }
    const uint32_t ecrc=crc32(entries,sizeof(entries));
    auto make=[&](uint8_t* s,uint64_t cur,uint64_t bak,uint64_t ent){ for(int i=0;i<8;++i)s[i]=sig[i]; wr32(s+8,0x00010000u);wr32(s+12,92);wr64(s+24,cur);wr64(s+32,bak);wr64(s+40,34);wr64(s+48,966);s[56]=0x42;wr64(s+72,ent);wr32(s+80,1);wr32(s+84,128);wr32(s+88,ecrc);wr32(s+16,0);wr32(s+16,crc32(s,92)); };
    make(psec,1,999,2); make(bsec,999,1,998);
    auto ph=parse_header(psec,sizeof(psec)); auto bh=parse_header(bsec,sizeof(bsec)); auto pt=parse_entries(ph,entries,sizeof(entries)); auto bt=parse_entries(bh,entries,sizeof(entries));
    auto pv=validate_copy(ph,pt,CopyKind::primary,999,512); auto bv=validate_copy(bh,bt,CopyKind::backup,999,512); auto rr=assess_redundancy(ph,pt,pv,bh,bt,bv,true);
    return pv.valid&&bv.valid&&rr.selected==Selection::primary&&!rr.degraded&&!rr.split_brain&&pt.used_entries==1&&pt.partitions[0].role==PartitionRole::system;
}
}
