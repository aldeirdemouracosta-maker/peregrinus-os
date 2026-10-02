#include "recovery_anchor.hpp"

namespace peregrinus::recovery_anchor {
namespace {
constexpr uint8_t kMagic[16]={'P','E','R','E','G','R','I','N','U','S','-','R','C','V','1',0};
constexpr uint32_t kHeaderSize=192;

static uint32_t rd32(const uint8_t* p){ return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
static uint64_t rd64(const uint8_t* p){ return uint64_t(rd32(p))|(uint64_t(rd32(p+4))<<32); }
static void wr32(uint8_t* p,uint32_t v){ for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(i*8)); }
static void wr64(uint8_t* p,uint64_t v){ for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(i*8)); }
static uint32_t crc32(const uint8_t* p,size_t n){ uint32_t c=0xFFFFFFFFu; for(size_t i=0;i<n;++i){ c^=p[i]; for(int b=0;b<8;++b)c=(c>>1)^(0xEDB88320u & (0u-(c&1u))); } return ~c; }
static bool bytes_equal(const uint8_t* a,const uint8_t* b,size_t n){ for(size_t i=0;i<n;++i)if(a[i]!=b[i])return false; return true; }
static void copy_guid(gpt::Guid& d,const uint8_t* s){ for(unsigned i=0;i<16;++i)d.bytes[i]=s[i]; }
static bool structurally_valid(const AnchorInfo& a){ return a.valid_magic&&a.valid_version&&a.valid_header_size&&a.valid_crc; }
static bool identities_equal(const AnchorInfo& a,const AnchorInfo& b){
    return gpt::guid_equal(a.disk_guid,b.disk_guid)&&gpt::guid_equal(a.recovery_guid,b.recovery_guid)&&
           gpt::guid_equal(a.system_guid,b.system_guid)&&gpt::guid_equal(a.data_guid,b.data_guid);
}
static bool geometry_equal(const AnchorInfo& a,const AnchorInfo& b){
    return a.system_first_lba==b.system_first_lba&&a.system_last_lba==b.system_last_lba&&
           a.recovery_first_lba==b.recovery_first_lba&&a.recovery_last_lba==b.recovery_last_lba&&
           a.data_first_lba==b.data_first_lba&&a.data_last_lba==b.data_last_lba;
}
static void make_test_anchor(uint8_t s[512],uint64_t generation,uint32_t flags,uint8_t identity_seed){
    for(unsigned i=0;i<512;++i)s[i]=0;
    for(unsigned i=0;i<16;++i)s[i]=kMagic[i];
    wr32(s+16,format_version); wr32(s+20,kHeaderSize); wr64(s+24,generation); wr32(s+32,flags); wr32(s+36,0);
    for(unsigned g=0;g<4;++g)for(unsigned i=0;i<16;++i)s[40+g*16+i]=uint8_t(identity_seed+g+i);
    wr64(s+104,2048);wr64(s+112,32767);wr64(s+120,32768);wr64(s+128,49151);wr64(s+136,49152);wr64(s+144,65501);
    const char* label="NOE-1.0-LKG"; for(unsigned i=0;label[i]&&i<32;++i)s[152+i]=(uint8_t)label[i];
    wr32(s+36,crc32(s,kHeaderSize));
}
}

AnchorInfo parse(const uint8_t* s,size_t bytes){
    AnchorInfo a{}; if(!s||bytes<kHeaderSize)return a;
    a.valid_magic=bytes_equal(s,kMagic,16); a.valid_version=rd32(s+16)==format_version;
    const uint32_t hs=rd32(s+20); a.valid_header_size=hs==kHeaderSize&&hs<=bytes;
    a.generation=rd64(s+24); const uint32_t flags=rd32(s+32); a.known_good=(flags&flag_known_good)!=0; a.clean_shutdown=(flags&flag_clean_shutdown)!=0;
    copy_guid(a.disk_guid,s+40);copy_guid(a.recovery_guid,s+56);copy_guid(a.system_guid,s+72);copy_guid(a.data_guid,s+88);
    a.system_first_lba=rd64(s+104);a.system_last_lba=rd64(s+112);a.recovery_first_lba=rd64(s+120);a.recovery_last_lba=rd64(s+128);a.data_first_lba=rd64(s+136);a.data_last_lba=rd64(s+144);
    for(unsigned i=0;i<32;++i){const char c=(char)s[152+i];a.label[i]=c;if(!c){for(unsigned j=i+1;j<33;++j)a.label[j]=0;break;}} a.label[32]=0;
    if(a.valid_header_size){ uint8_t tmp[kHeaderSize]; for(unsigned i=0;i<kHeaderSize;++i)tmp[i]=s[i]; const uint32_t want=rd32(tmp+36); wr32(tmp+36,0); a.valid_crc=crc32(tmp,kHeaderSize)==want; }
    return a;
}

Assessment assess(const AnchorInfo& first,bool first_layout_ok,const AnchorInfo& last,bool last_layout_ok){
    Assessment r{}; r.first_valid=structurally_valid(first)&&first_layout_ok; r.last_valid=structurally_valid(last)&&last_layout_ok;
    if(r.first_valid&&r.last_valid){
        r.identities_match=identities_equal(first,last); r.geometry_match=geometry_equal(first,last);
        if(!r.identities_match||!r.geometry_match){r.split_brain=true;return r;}
        const bool fg=first.known_good,lg=last.known_good;
        if(fg&&lg){ if(first.generation>=last.generation){r.selected=Copy::first;r.selected_generation=first.generation;}else{r.selected=Copy::last;r.selected_generation=last.generation;} r.rolling_update=first.generation!=last.generation; r.last_known_good=true; r.selected_clean_shutdown=(r.selected==Copy::first?first.clean_shutdown:last.clean_shutdown); return r; }
        if(fg!=lg){r.selected=fg?Copy::first:Copy::last;r.selected_generation=fg?first.generation:last.generation;r.degraded=true;r.last_known_good=true;r.selected_clean_shutdown=fg?first.clean_shutdown:last.clean_shutdown;return r;}
        r.degraded=true; return r;
    }
    if(r.first_valid&&first.known_good){r.selected=Copy::first;r.selected_generation=first.generation;r.degraded=true;r.last_known_good=true;r.selected_clean_shutdown=first.clean_shutdown;return r;}
    if(r.last_valid&&last.known_good){r.selected=Copy::last;r.selected_generation=last.generation;r.degraded=true;r.last_known_good=true;r.selected_clean_shutdown=last.clean_shutdown;return r;}
    r.degraded=true; return r;
}

const char* copy_name(Copy c){switch(c){case Copy::first:return "FIRST";case Copy::last:return "LAST";default:return "NONE";}}

bool self_test(){
    uint8_t a[512],b[512];make_test_anchor(a,7,flag_known_good|flag_clean_shutdown,0x10);make_test_anchor(b,7,flag_known_good|flag_clean_shutdown,0x10);
    auto pa=parse(a,sizeof(a));auto pb=parse(b,sizeof(b));auto same=assess(pa,true,pb,true);
    if(!same.first_valid||!same.last_valid||same.selected!=Copy::first||same.degraded||same.split_brain||!same.last_known_good)return false;
    b[40]^=1;wr32(b+36,0);wr32(b+36,crc32(b,kHeaderSize));pb=parse(b,sizeof(b));auto split=assess(pa,true,pb,true);if(!split.split_brain||split.selected!=Copy::none)return false;
    make_test_anchor(b,8,flag_known_good,0x10);pb=parse(b,sizeof(b));auto rolling=assess(pa,true,pb,true);if(rolling.selected!=Copy::last||!rolling.rolling_update||rolling.selected_generation!=8)return false;
    a[12]^=1;pa=parse(a,sizeof(a));auto degraded=assess(pa,true,pb,true);return !degraded.first_valid&&degraded.last_valid&&degraded.selected==Copy::last&&degraded.degraded&&degraded.last_known_good;
}

}
