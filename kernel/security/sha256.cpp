#include "sha256.hpp"

namespace peregrinus::security::sha256 {
namespace {
constexpr uint32_t k[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t rotr(uint32_t x, unsigned n){ return (x >> n) | (x << (32u - n)); }
static uint32_t rdbe32(const uint8_t* p){ return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]); }
static void wrbe32(uint8_t* p,uint32_t v){ p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v); }
static void wrbe64(uint8_t* p,uint64_t v){ for(unsigned i=0;i<8;++i)p[7-i]=uint8_t(v>>(i*8)); }

static void transform(Context& c,const uint8_t block[64]){
    uint32_t w[64];
    for(unsigned i=0;i<16;++i) w[i]=rdbe32(block+i*4);
    for(unsigned i=16;i<64;++i){
        const uint32_t s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
        const uint32_t s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=c.state[0],b=c.state[1],cc=c.state[2],d=c.state[3],e=c.state[4],f=c.state[5],g=c.state[6],h=c.state[7];
    for(unsigned i=0;i<64;++i){
        const uint32_t s1=rotr(e,6)^rotr(e,11)^rotr(e,25);
        const uint32_t ch=(e&f)^((~e)&g);
        const uint32_t t1=h+s1+ch+k[i]+w[i];
        const uint32_t s0=rotr(a,2)^rotr(a,13)^rotr(a,22);
        const uint32_t maj=(a&b)^(a&cc)^(b&cc);
        const uint32_t t2=s0+maj;
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c.state[0]+=a;c.state[1]+=b;c.state[2]+=cc;c.state[3]+=d;c.state[4]+=e;c.state[5]+=f;c.state[6]+=g;c.state[7]+=h;
}
}

void init(Context& c){
    c.state[0]=0x6a09e667u;c.state[1]=0xbb67ae85u;c.state[2]=0x3c6ef372u;c.state[3]=0xa54ff53au;
    c.state[4]=0x510e527fu;c.state[5]=0x9b05688cu;c.state[6]=0x1f83d9abu;c.state[7]=0x5be0cd19u;
    c.total_bytes=0;c.block_used=0;for(unsigned i=0;i<64;++i)c.block[i]=0;
}

void update(Context& c,const void* data,size_t bytes){
    const auto* p=static_cast<const uint8_t*>(data); if(!p&&bytes)return;
    c.total_bytes+=bytes;
    while(bytes){
        const uint32_t room=64u-c.block_used;
        const uint32_t take=bytes<room?uint32_t(bytes):room;
        for(uint32_t i=0;i<take;++i)c.block[c.block_used+i]=p[i];
        c.block_used+=take;p+=take;bytes-=take;
        if(c.block_used==64){transform(c,c.block);c.block_used=0;}
    }
}

void final(Context& c,uint8_t out[32]){
    const uint64_t bits=c.total_bytes*8u;
    c.block[c.block_used++]=0x80;
    if(c.block_used>56){while(c.block_used<64)c.block[c.block_used++]=0;transform(c,c.block);c.block_used=0;}
    while(c.block_used<56)c.block[c.block_used++]=0;
    wrbe64(c.block+56,bits);transform(c,c.block);c.block_used=0;
    for(unsigned i=0;i<8;++i)wrbe32(out+i*4,c.state[i]);
}

void digest(const void* data,size_t bytes,uint8_t out[32]){ Context c{};init(c);update(c,data,bytes);final(c,out); }
bool equal(const uint8_t a[32],const uint8_t b[32]){ uint8_t d=0;for(unsigned i=0;i<32;++i)d|=uint8_t(a[i]^b[i]);return d==0; }

bool self_test(){
    static constexpr uint8_t empty_expected[32]={0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55};
    static constexpr uint8_t abc_expected[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t out[32]; digest(nullptr,0,out); if(!equal(out,empty_expected))return false;
    const char abc[3]={'a','b','c'};digest(abc,3,out);return equal(out,abc_expected);
}
}
