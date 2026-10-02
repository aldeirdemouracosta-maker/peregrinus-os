#include "stateful_guard.hpp"

namespace peregrinus::net::stateful {
namespace {
using namespace security::firewall;
bool trackable(Protocol p){return p==Protocol::tcp||p==Protocol::udp;}
bool same_origin(const Flow& f,const PacketV4& p){return f.used&&f.protocol==p.protocol&&f.origin_direction==p.direction&&f.origin_src==p.src&&f.origin_dst==p.dst&&f.origin_src_port==p.src_port&&f.origin_dst_port==p.dst_port;}
bool reverse_match(const Flow& f,const PacketV4& p){return f.used&&f.protocol==p.protocol&&f.origin_direction!=p.direction&&f.origin_src==p.dst&&f.origin_dst==p.src&&f.origin_src_port==p.dst_port&&f.origin_dst_port==p.src_port;}
bool tcp_reply_safe(const PacketV4& p){
    if(p.protocol!=Protocol::tcp)return true;
    constexpr uint8_t SYN=0x02,ACK=0x10,RST=0x04;
    if((p.tcp_flags&RST)!=0)return true;
    if((p.tcp_flags&ACK)!=0)return true;
    return (p.tcp_flags&(SYN|ACK))==(SYN|ACK);
}
}
void Guard::reset(){for(auto& f:flows_)f={};replacement_=0;}
void Guard::observe_allowed(const PacketV4& p,uint64_t seq){
    if(!trackable(p.protocol)||p.fragmented)return;
    for(auto& f:flows_)if(same_origin(f,p)){f.last_sequence=seq;return;}
    for(auto& f:flows_)if(!f.used){f={true,p.protocol,p.direction,p.src,p.dst,p.src_port,p.dst_port,seq};return;}
    flows_[replacement_]={true,p.protocol,p.direction,p.src,p.dst,p.src_port,p.dst_port,seq};replacement_=(replacement_+1)%capacity;
}
bool Guard::allow_reply(const PacketV4& p,uint64_t seq){
    if(!trackable(p.protocol)||p.fragmented||!tcp_reply_safe(p))return false;
    for(auto& f:flows_){
        if(!reverse_match(f,p))continue;
        if(seq<f.last_sequence||seq-f.last_sequence>event_ttl){f={};return false;}
        f.last_sequence=seq;return true;
    }
    return false;
}
size_t Guard::active_count(uint64_t seq)const{size_t n=0;for(const auto& f:flows_)if(f.used&&seq>=f.last_sequence&&seq-f.last_sequence<=event_ttl)++n;return n;}
bool self_test(){
    Guard g;g.reset();PacketV4 out{Direction::egress,Protocol::tcp,0x0a00020f,0x01010101,50000,443,false,0x02,0,0};g.observe_allowed(out,10);
    PacketV4 in{Direction::ingress,Protocol::tcp,0x01010101,0x0a00020f,443,50000,false,0x12,0,0};if(!g.allow_reply(in,11))return false;
    PacketV4 syn=in;syn.tcp_flags=0x02;if(g.allow_reply(syn,12))return false;
    PacketV4 wrong=in;wrong.src_port=444;if(g.allow_reply(wrong,12))return false;
    PacketV4 udp{Direction::egress,Protocol::udp,0x0a00020f,0x0a000203,53000,53,false,0,0,0};g.observe_allowed(udp,20);PacketV4 ur{Direction::ingress,Protocol::udp,0x0a000203,0x0a00020f,53,53000,false,0,0,0};if(!g.allow_reply(ur,21))return false;
    if(g.allow_reply(ur,21+event_ttl+1))return false;return true;
}
}
