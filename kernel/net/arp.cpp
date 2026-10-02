#include "arp.hpp"

namespace peregrinus::net::arp {
namespace {
uint16_t be16(const uint8_t* p){return static_cast<uint16_t>((uint16_t(p[0])<<8)|p[1]);}
uint32_t be32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
void put16(uint8_t* p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}
void put32(uint8_t* p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
void mac_copy(uint8_t* d,const e1000::MacAddress& m){for(size_t i=0;i<6;++i)d[i]=m.bytes[i];}
e1000::MacAddress mac_read(const uint8_t* p){e1000::MacAddress m{};for(size_t i=0;i<6;++i)m.bytes[i]=p[i];return m;}
}
Status parse(const uint8_t* frame,size_t bytes,Packet& out){
    out={}; if(!frame||bytes<42)return Status::too_short;
    if(be16(frame+12)!=0x0806)return Status::not_arp;
    const uint8_t* a=frame+14;
    if(be16(a)!=1||be16(a+2)!=0x0800||a[4]!=6||a[5]!=4)return Status::unsupported;
    const uint16_t op=be16(a+6); if(op!=1&&op!=2)return Status::invalid;
    out.opcode=op;out.sender_mac=mac_read(a+8);out.sender_ip=be32(a+14);out.target_mac=mac_read(a+18);out.target_ip=be32(a+24);return Status::ok;
}
size_t build_reply(const Packet& req,const e1000::MacAddress& local_mac,uint32_t local_ip,uint8_t* out,size_t cap){
    if(!out||cap<42||req.opcode!=1||req.target_ip!=local_ip)return 0;
    for(size_t i=0;i<42;++i)out[i]=0;
    mac_copy(out,req.sender_mac);mac_copy(out+6,local_mac);put16(out+12,0x0806);
    uint8_t* a=out+14;put16(a,1);put16(a+2,0x0800);a[4]=6;a[5]=4;put16(a+6,2);
    mac_copy(a+8,local_mac);put32(a+14,local_ip);mac_copy(a+18,req.sender_mac);put32(a+24,req.sender_ip);return 42;
}
bool self_test(){
    uint8_t f[42]{}; for(int i=0;i<6;++i){f[i]=0xff;f[6+i]=uint8_t(i+1);}put16(f+12,0x0806);uint8_t* a=f+14;put16(a,1);put16(a+2,0x0800);a[4]=6;a[5]=4;put16(a+6,1);for(int i=0;i<6;++i)a[8+i]=uint8_t(i+1);put32(a+14,0x0a000202);put32(a+24,0x0a00020f);
    Packet p{};if(parse(f,sizeof(f),p)!=Status::ok||p.opcode!=1||p.target_ip!=0x0a00020f)return false;
    e1000::MacAddress m{{0x52,0x54,0,0x12,0x34,0x56}};uint8_t r[64]{};size_t n=build_reply(p,m,0x0a00020f,r,sizeof(r));if(n!=42)return false;Packet q{};if(parse(r,n,q)!=Status::ok||q.opcode!=2||q.sender_ip!=0x0a00020f||q.target_ip!=0x0a000202)return false;return true;
}
}
