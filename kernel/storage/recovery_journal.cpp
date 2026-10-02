#include "recovery_journal.hpp"
namespace peregrinus::recovery_journal {
namespace {
static bool bytes_equal(const void* a,const void* b,uint32_t n){const auto* x=(const uint8_t*)a;const auto* y=(const uint8_t*)b;for(uint32_t i=0;i<n;++i)if(x[i]!=y[i])return false;return true;}
static void copy_record(peregrinus_recovery_journal& d,const void* s){const auto* p=(const uint8_t*)s;auto* q=(uint8_t*)&d;for(uint32_t i=0;i<sizeof(d);++i)q[i]=p[i];}
static bool matches(const peregrinus_recovery_journal& j,const gpt::Guid& disk,const volume::Volume& rec){return peregrinus_rj_identity(&j,disk.bytes,rec.unique_guid.bytes)!=0;}
}
Assessment assess(const void* a512,const void* b512,const gpt::Guid& disk_guid,const volume::Volume& recovery){
    Assessment out{};peregrinus_recovery_journal a{},b{};if(a512)copy_record(a,a512);if(b512)copy_record(b,b512);out.a_valid=a512&&matches(a,disk_guid,recovery);out.b_valid=b512&&matches(b,disk_guid,recovery);
    if(out.a_valid&&out.b_valid){if(a.sequence==b.sequence&&!bytes_equal(&a,&b,sizeof(a))){out.split_brain=true;return out;}if(a.sequence>=b.sequence){out.selected=Copy::a;out.record=a;}else{out.selected=Copy::b;out.record=b;}return out;}
    if(out.a_valid){out.selected=Copy::a;out.record=a;}else if(out.b_valid){out.selected=Copy::b;out.record=b;}return out;
}
Copy inactive_copy(Copy active){if(active==Copy::a)return Copy::b;if(active==Copy::b)return Copy::a;return Copy::none;}
const char* copy_name(Copy c){return c==Copy::a?"A":c==Copy::b?"B":"NONE";}
bool self_test(){
    gpt::Guid dg{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}},rg{{16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1}};volume::Volume rec{};rec.unique_guid=rg;
    uint8_t asec[512]{},bsec[512]{};auto* a=(peregrinus_recovery_journal*)asec;auto* b=(peregrinus_recovery_journal*)bsec;peregrinus_rj_init(a,dg.bytes,rg.bytes,8,7,3,3);*b=*a;b->sequence=2;b->crc32=0;b->crc32=peregrinus_rj_crc(b);
    auto x=assess(asec,bsec,dg,rec);if(x.selected!=Copy::b||x.split_brain)return false;
    if(!peregrinus_rj_prepare_attempt(&x.record,PEREGRINUS_RJ_SLOT_CURRENT)||x.record.current_attempts!=1)return false;
    b->crc32^=1;x=assess(asec,bsec,dg,rec);if(x.selected!=Copy::a)return false;
    *b=*a;b->crc32=0;b->crc32=peregrinus_rj_crc(b);b->lkg_attempts=1;b->crc32=0;b->crc32=peregrinus_rj_crc(b);x=assess(asec,bsec,dg,rec);if(!x.split_brain)return false;
    return inactive_copy(Copy::a)==Copy::b&&inactive_copy(Copy::b)==Copy::a;
}
}
