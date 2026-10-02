#include "datapath.hpp"
namespace peregrinus::net {
namespace {
using namespace security::firewall;
bool new_ingress_candidate(const PacketV4& p){
    if(p.direction!=Direction::ingress)return false;
    if(p.protocol==Protocol::tcp){constexpr uint8_t SYN=0x02,ACK=0x10;return (p.tcp_flags&SYN)!=0&&(p.tcp_flags&ACK)==0;}
    return p.protocol==Protocol::udp;
}
}
void Datapath::reset(){engine_.reset();stateful_.reset();burst_.reset();audit_head_=audit_count_=0;sequence_=allowed_=denied_=0;for(auto& a:audit_)a={};}
void Datapath::record(security::firewall::Direction d,size_t n,const DatapathDecision& x){DatapathAudit a{sequence_,d,n,x};audit_[audit_head_]=a;audit_head_=(audit_head_+1)%datapath_audit_capacity;if(audit_count_<datapath_audit_capacity)++audit_count_;}
DatapathDecision Datapath::inspect(security::firewall::Direction direction,const uint8_t* frame,size_t bytes){
    ++sequence_;const auto p=parse_ethernet_ipv4(direction,frame,bytes);DatapathDecision out{security::firewall::Verdict::deny,DatapathReason::parse_reject,p.status,0};
    if(!p.accepted){++denied_;record(direction,bytes,out);return out;}
    const auto fw=engine_.evaluate(p.packet);
    if(fw.verdict==security::firewall::Verdict::allow){
        if(new_ingress_candidate(p.packet)&&!burst_.admit(p.packet.src,sequence_)){out={security::firewall::Verdict::deny,DatapathReason::burst_guard,ParseStatus::ok,fw.rule_id};++denied_;record(direction,bytes,out);return out;}
        stateful_.observe_allowed(p.packet,sequence_);out={security::firewall::Verdict::allow,DatapathReason::explicit_allow,ParseStatus::ok,fw.rule_id};++allowed_;record(direction,bytes,out);return out;
    }
    if(stateful_.allow_reply(p.packet,sequence_)){out={security::firewall::Verdict::allow,DatapathReason::stateful_reply,ParseStatus::ok,0};++allowed_;record(direction,bytes,out);return out;}
    out={security::firewall::Verdict::deny,DatapathReason::default_deny,ParseStatus::ok,0};++denied_;record(direction,bytes,out);return out;
}
bool Datapath::audit_get_oldest(size_t i,DatapathAudit& out)const{if(i>=audit_count_)return false;const size_t oldest=(audit_head_+datapath_audit_capacity-audit_count_)%datapath_audit_capacity;out=audit_[(oldest+i)%datapath_audit_capacity];return true;}
const char* datapath_reason_name(DatapathReason r){switch(r){case DatapathReason::parse_reject:return"PARSE-REJECT";case DatapathReason::explicit_allow:return"EXPLICIT-ALLOW";case DatapathReason::stateful_reply:return"STATEFUL-REPLY";case DatapathReason::default_deny:return"DEFAULT-DENY";case DatapathReason::burst_guard:return"BURST-GUARD";}return"UNKNOWN";}

static uint16_t be16(const uint8_t*p){return uint16_t((uint16_t(p[0])<<8)|p[1]);}
static void put16(uint8_t*p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}static uint16_t csum(const uint8_t*p,size_t n){uint32_t s=0;for(size_t i=0;i<n;i+=2)s+=be16(p+i);while(s>>16)s=(s&0xffffu)+(s>>16);return uint16_t(~s);}
static size_t udp_frame(uint8_t*f,uint32_t s,uint32_t d,uint16_t sp,uint16_t dp){for(size_t i=0;i<42;++i)f[i]=0;f[12]=8;f[13]=0;uint8_t*ip=f+14;ip[0]=0x45;put16(ip+2,28);ip[8]=64;ip[9]=17;for(int i=0;i<4;++i){ip[12+i]=uint8_t(s>>(24-8*i));ip[16+i]=uint8_t(d>>(24-8*i));}put16(ip+10,csum(ip,20));uint8_t*u=ip+20;put16(u,sp);put16(u+2,dp);put16(u+4,8);return 42;}
bool datapath_self_test(){
    using namespace security::firewall;Datapath d;d.reset();AllowRuleV4 dns{7,true,Direction::egress,Protocol::udp,ipv4(10,0,2,0),24,ipv4(10,0,2,3),32,1024,65535,53,53};if(!d.add_allow_rule(dns))return false;uint8_t f[64]{};size_t n=udp_frame(f,ipv4(10,0,2,15),ipv4(10,0,2,3),53000,53);auto x=d.inspect(Direction::egress,f,n);if(x.verdict!=Verdict::allow||x.reason!=DatapathReason::explicit_allow)return false;n=udp_frame(f,ipv4(10,0,2,3),ipv4(10,0,2,15),53,53000);x=d.inspect(Direction::ingress,f,n);if(x.verdict!=Verdict::allow||x.reason!=DatapathReason::stateful_reply)return false;n=udp_frame(f,ipv4(9,9,9,9),ipv4(10,0,2,15),1000,9999);x=d.inspect(Direction::ingress,f,n);if(x.verdict!=Verdict::deny)return false;return d.audit_count()==3&&d.allowed()==2&&d.denied()==1;
}
}
