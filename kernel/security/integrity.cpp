#include "integrity.hpp"
#include "sha256.hpp"

extern "C" {
extern const uint8_t __text_start[];
extern const uint8_t __text_end[];
extern const uint8_t __integrity_manifest_start[];
extern const uint8_t __integrity_manifest_end[];
}

namespace peregrinus::security::integrity {
namespace {
constexpr uint8_t kMagic[16]={'P','E','R','E','G','R','I','N','U','S','-','G','U','A','R','D'};
__attribute__((used,section(".peregrinus_integrity"),aligned(16)))
const uint8_t g_manifest_placeholder[manifest_bytes]={0};

static uint32_t rd32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
static uint64_t rd64(const uint8_t* p){return uint64_t(rd32(p))|(uint64_t(rd32(p+4))<<32);}
static bool bytes_equal(const uint8_t* a,const uint8_t* b,size_t n){uint8_t d=0;for(size_t i=0;i<n;++i)d|=uint8_t(a[i]^b[i]);return d==0;}
static void copy32(uint8_t* d,const uint8_t* s){for(unsigned i=0;i<32;++i)d[i]=s[i];}
static void copy_label(char d[33],const uint8_t* s){unsigned i=0;for(;i<32&&s[i];++i)d[i]=char(s[i]);for(;i<33;++i)d[i]=0;}
}

Manifest parse_manifest(const uint8_t* p,size_t size){
    Manifest m{};if(!p||size<manifest_bytes)return m;
    m.valid_magic=bytes_equal(p,kMagic,16);m.valid_version=rd32(p+16)==manifest_version;m.valid_size=rd32(p+20)==manifest_bytes;
    m.text_size=rd64(p+24);copy32(m.text_sha256,p+32);copy_label(m.label,p+64);return m;
}

Report verify_kernel_text(){
    Report r{};
    const size_t mbytes=size_t(__integrity_manifest_end-__integrity_manifest_start);
    const auto m=parse_manifest(__integrity_manifest_start,mbytes);
    r.manifest_valid=m.valid_magic&&m.valid_version&&m.valid_size;
    const uint64_t tsize=uint64_t(__text_end-__text_start);r.text_size=tsize;r.text_size_match=r.manifest_valid&&m.text_size==tsize;
    copy32(r.expected,m.text_sha256);copy_label(r.label,reinterpret_cast<const uint8_t*>(m.label));
    sha256::digest(__text_start,size_t(tsize),r.computed);r.digest_match=r.text_size_match&&sha256::equal(r.expected,r.computed);return r;
}

bool self_test(){
    uint8_t b[manifest_bytes]={};for(unsigned i=0;i<16;++i)b[i]=kMagic[i];
    b[16]=1;b[20]=uint8_t(manifest_bytes);b[24]=3;for(unsigned i=0;i<32;++i)b[32+i]=uint8_t(i);b[64]='J';b[65]='O';b[66]='B';
    const auto m=parse_manifest(b,sizeof(b));return m.valid_magic&&m.valid_version&&m.valid_size&&m.text_size==3&&m.text_sha256[31]==31&&m.label[0]=='J'&&m.label[2]=='B';
}
}
