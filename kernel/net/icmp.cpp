#include "icmp.hpp"

namespace peregrinus::net::icmp {
namespace {
uint16_t be16(const uint8_t* p){return uint16_t((uint16_t(p[0])<<8)|p[1]);}
uint32_t be32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
void put16(uint8_t* p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}
void put32(uint8_t* p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
uint16_t sum16(const uint8_t* p,size_t n){uint32_t s=0;size_t i=0;for(;i+1<n;i+=2)s+=be16(p+i);if(i<n)s+=uint16_t(p[i])<<8;while(s>>16)s=(s&0xffffu)+(s>>16);return uint16_t(~s);}
bool checksum_ok(const uint8_t* p,size_t n){uint32_t s=0;size_t i=0;for(;i+1<n;i+=2)s+=be16(p+i);if(i<n)s+=uint16_t(p[i])<<8;while(s>>16)s=(s&0xffffu)+(s>>16);return uint16_t(s)==0xffffu;}
void mac_copy(uint8_t* d,const e1000::MacAddress& m){for(size_t i=0;i<6;++i)d[i]=m.bytes[i];}
}
// Never answer (reflect to) a source that cannot be a single remote host: unspecified,
// loopback, multicast, limited broadcast, or our own address.
bool unicast_source(uint32_t src,uint32_t local_ip){
    if(src==0||src==local_ip||src==0xffffffffu)return false;
    if((src>>24)==127u||(src>>28)==0xeu)return false;
    return true;
}
size_t build_echo_reply(const uint8_t* req,size_t n,const e1000::MacAddress& local_mac,uint32_t local_ip,uint8_t* out,size_t cap){
    if(!req||!out||n<42||cap<n||be16(req+12)!=0x0800)return 0;const uint8_t* ip=req+14;if(ip[0]!=0x45||ip[9]!=1)return 0;const uint16_t total=be16(ip+2);if(total<28||size_t(total)+14>n)return 0;if(be32(ip+16)!=local_ip)return 0;if(!unicast_source(be32(ip+12),local_ip))return 0;const uint8_t* ic=ip+20;const size_t icn=total-20;if(ic[0]!=8||ic[1]!=0||!checksum_ok(ic,icn))return 0;
    for(size_t i=0;i<14+total;++i)out[i]=req[i];for(size_t i=0;i<6;++i)out[i]=req[6+i];mac_copy(out+6,local_mac);uint8_t* oip=out+14;const uint32_t src=be32(ip+12);put32(oip+12,local_ip);put32(oip+16,src);oip[8]=64;put16(oip+10,0);put16(oip+10,sum16(oip,20));uint8_t* oic=oip+20;oic[0]=0;put16(oic+2,0);put16(oic+2,sum16(oic,icn));return 14+total;
}
bool self_test(){
    e1000::MacAddress local{{0x52,0x54,0,0x12,0x34,0x56}};uint8_t f[64]{};for(int i=0;i<6;++i){f[i]=local.bytes[i];f[6+i]=uint8_t(i+1);}put16(f+12,0x0800);uint8_t* ip=f+14;ip[0]=0x45;put16(ip+2,28);ip[8]=64;ip[9]=1;put32(ip+12,0x0a000202);put32(ip+16,0x0a00020f);put16(ip+10,sum16(ip,20));uint8_t* ic=ip+20;ic[0]=8;ic[1]=0;put16(ic+4,1);put16(ic+6,2);put16(ic+2,sum16(ic,8));uint8_t out[64]{};const size_t m=build_echo_reply(f,42,local,0x0a00020f,out,sizeof(out));if(!(m==42&&out[34]==0&&checksum_ok(out+34,8)&&be32(out+26)==0x0a00020f&&be32(out+30)==0x0a000202))return false;
    if(out[14+8]!=64||!checksum_ok(out+14,20))return false;                       // fresh TTL, valid IP checksum
    const uint32_t bad_src[]={0u,0x0a00020fu,0xffffffffu,0x7f000001u,0xe0000001u};
    for(uint32_t b:bad_src){uint8_t g[64]{};for(int i=0;i<42;++i)g[i]=f[i];put32(g+26,b);put16(g+24,0);put16(g+24,sum16(g+14,20));if(build_echo_reply(g,42,local,0x0a00020f,out,sizeof(out))!=0)return false;}
    return true;
}
}
