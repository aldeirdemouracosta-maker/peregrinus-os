#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "kernel/net/datapath.hpp"
#include "kernel/net/e1000_model.hpp"

static uint32_t rng_state=0x71c0ffeeu;
// xorshift32: unlike a power-of-two LCG its low bits are not periodic, so `% k` choices are usable.
static uint32_t next_u32(){rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state;}

int main(){
    using namespace peregrinus;
    net::Datapath d;d.reset();
    uint8_t frame[1600]{};
    for(unsigned iter=0;iter<12000;++iter){
        const size_t n=static_cast<size_t>(next_u32()%sizeof(frame));
        for(size_t i=0;i<n;++i)frame[i]=static_cast<uint8_t>(next_u32()>>24);
        const auto dir=(next_u32()&1)?security::firewall::Direction::ingress:security::firewall::Direction::egress;
        const auto x=d.inspect(dir,frame,n);
        if(x.verdict!=security::firewall::Verdict::deny){
            std::puts("FAIL: default-deny adversarial frame escaped");return 1;
        }
    }
    if(d.audit_count()!=net::datapath_audit_capacity){std::puts("FAIL: audit ring did not saturate safely");return 2;}
    // Structured frames: random bytes almost never get past the EtherType check, so also build
    // well-formed Ethernet/IPv4 frames (valid header checksum) with random header fields and
    // payloads. With a single ICMP-ingress rule, nothing but ICMP to 10.0.2.15 may be allowed.
    net::Datapath p;p.reset();
    using namespace security::firewall;
    const AllowRuleV4 icmp_in{1,true,Direction::ingress,Protocol::icmp,0,0,ipv4(10,0,2,15),32,0,65535,0,65535};
    if(!p.add_allow_rule(icmp_in)){std::puts("FAIL: rule");return 4;}
    unsigned allowed=0;
    for(unsigned iter=0;iter<60000;++iter){
        const size_t n=34+static_cast<size_t>(next_u32()%1480);
        for(size_t i=0;i<n;++i)frame[i]=static_cast<uint8_t>(next_u32()>>24);
        frame[12]=0x08;frame[13]=0x00;uint8_t* ip=frame+14;
        ip[0]=(next_u32()%8)?0x45:static_cast<uint8_t>(next_u32());
        const uint16_t total=static_cast<uint16_t>((next_u32()%4)?n-14:next_u32());ip[2]=uint8_t(total>>8);ip[3]=uint8_t(total);
        const uint8_t protos[]={1,6,17,static_cast<uint8_t>(next_u32())};ip[9]=protos[next_u32()%4];
        if(next_u32()%2){ip[6]=0;ip[7]=0;}
        if(next_u32()%2){ip[16]=10;ip[17]=0;ip[18]=2;ip[19]=15;}
        ip[10]=ip[11]=0;uint32_t sum=0;for(int i=0;i<20;i+=2)sum+=uint32_t(ip[i]<<8|ip[i+1]);while(sum>>16)sum=(sum&0xffff)+(sum>>16);
        if(next_u32()%16){ip[10]=uint8_t(~sum>>8);ip[11]=uint8_t(~sum);}
        const auto x=p.inspect(Direction::ingress,frame,n);
        if(x.verdict==Verdict::allow){
            ++allowed;
            const bool icmp_to_local=ip[9]==1&&ip[16]==10&&ip[17]==0&&ip[18]==2&&ip[19]==15&&(ip[6]&0x3f)==0&&ip[7]==0;
            if(!icmp_to_local){std::puts("FAIL: structured frame allowed outside the ICMP rule");return 5;}
        }
    }
    if(allowed==0){std::puts("FAIL: structured generator never reached the allow path");return 6;}
    for(unsigned i=0;i<5000;++i){
        net::e1000::RxDescriptor r{};r.length=static_cast<uint16_t>(next_u32());r.status=static_cast<uint8_t>(next_u32());r.errors=static_cast<uint8_t>(next_u32());
        const bool valid=net::e1000::rx_frame_valid(r);
        if(valid && (r.length<14||r.length>net::e1000::buffer_bytes||(r.status&net::e1000::rx_status_dd)==0||(r.status&net::e1000::rx_status_eop)==0||r.errors!=0)){
            std::puts("FAIL: invalid RX descriptor accepted");return 3;
        }
    }
    std::puts("Muro adversarial parser/ring test: PASS");return 0;
}
