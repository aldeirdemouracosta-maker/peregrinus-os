#include "sandbox_service.hpp"
#include "arp.hpp"
#include "icmp.hpp"
#include "../security/firewall.hpp"
#include "../console/serial.hpp"

namespace peregrinus::net::sandbox {
namespace { Service g_service{}; }

bool Service::init(){
    stats_={};datapath_.reset();nic_=&e1000::driver();const auto st=nic_->init_qemu_sandbox();serial::write("e1000 sandbox init: ");serial::writeln(e1000::init_status_name(st));if(st!=e1000::InitStatus::ready)return false;mac_=nic_->mac();
    using namespace security::firewall;
    const AllowRuleV4 icmp_in{3001,true,Direction::ingress,Protocol::icmp,0,0,local_ip_,32,0,65535,0,65535};
    const AllowRuleV4 icmp_out{3002,true,Direction::egress,Protocol::icmp,local_ip_,32,0,0,0,65535,0,65535};
    if(!datapath_.add_allow_rule(icmp_in)||!datapath_.add_allow_rule(icmp_out))return false;
    serial::writeln("Network qualification: static 10.0.2.15, ARP + ICMP echo only");return true;
}

bool Service::consume(const uint8_t* frame,size_t bytes,void* context){return static_cast<Service*>(context)->on_frame(frame,bytes);}

bool Service::on_frame(const uint8_t* frame,size_t bytes){
    arp::Packet ap{};const auto as=arp::parse(frame,bytes,ap);if(as==arp::Status::ok){
        if(ap.opcode==1&&ap.target_ip==local_ip_){++stats_.arp_requests;uint8_t reply[64]{};const size_t n=arp::build_reply(ap,mac_,local_ip_,reply,sizeof(reply));if(n&&nic_->transmit(reply,n)){++stats_.arp_replies_sent;return true;}++stats_.tx_failures;}
        return false;
    }
    const auto in=datapath_.inspect(security::firewall::Direction::ingress,frame,bytes);if(in.verdict!=security::firewall::Verdict::allow){++stats_.ipv4_denied;return false;}++stats_.ipv4_allowed;
    uint8_t reply[1514]{};const size_t n=icmp::build_echo_reply(frame,bytes,mac_,local_ip_,reply,sizeof(reply));if(!n)return true;
    const auto out=datapath_.inspect(security::firewall::Direction::egress,reply,n);if(out.verdict!=security::firewall::Verdict::allow){++stats_.tx_failures;return false;}
    if(!nic_->transmit(reply,n)){++stats_.tx_failures;return false;}++stats_.icmp_replies_sent;return true;
}

size_t Service::poll(size_t budget){return nic_?nic_->poll_rx(&Service::consume,this,budget):0;}
Service& service(){return g_service;}
}
