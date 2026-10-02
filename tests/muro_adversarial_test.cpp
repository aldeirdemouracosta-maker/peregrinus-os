#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "kernel/net/datapath.hpp"
#include "kernel/net/e1000_model.hpp"

static uint32_t rng_state=0x71c0ffeeu;
static uint32_t next_u32(){rng_state=rng_state*1664525u+1013904223u;return rng_state;}

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
    for(unsigned i=0;i<5000;++i){
        net::e1000::RxDescriptor r{};r.length=static_cast<uint16_t>(next_u32());r.status=static_cast<uint8_t>(next_u32());r.errors=static_cast<uint8_t>(next_u32());
        const bool valid=net::e1000::rx_frame_valid(r);
        if(valid && (r.length<14||r.length>net::e1000::buffer_bytes||(r.status&net::e1000::rx_status_dd)==0||(r.status&net::e1000::rx_status_eop)==0||r.errors!=0)){
            std::puts("FAIL: invalid RX descriptor accepted");return 3;
        }
    }
    std::puts("Muro adversarial parser/ring test: PASS");return 0;
}
