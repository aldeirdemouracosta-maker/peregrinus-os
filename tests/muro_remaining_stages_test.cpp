#include <cstdio>
#include "kernel/net/e1000_model.hpp"
#include "kernel/net/arp.hpp"
#include "kernel/net/icmp.hpp"
#include "kernel/net/stateful_guard.hpp"
#include "kernel/net/burst_guard.hpp"
#include "kernel/net/datapath.hpp"

int main(){
    if(!peregrinus::net::e1000::model_self_test()){std::puts("e1000 model FAIL");return 1;}
    if(!peregrinus::net::arp::self_test()){std::puts("arp FAIL");return 2;}
    if(!peregrinus::net::icmp::self_test()){std::puts("icmp FAIL");return 3;}
    if(!peregrinus::net::stateful::self_test()){std::puts("stateful FAIL");return 4;}
    if(!peregrinus::net::burst::self_test()){std::puts("burst FAIL");return 5;}
    if(!peregrinus::net::datapath_self_test()){std::puts("datapath FAIL");return 6;}
    std::puts("Muro remaining stages: PASS");return 0;
}
