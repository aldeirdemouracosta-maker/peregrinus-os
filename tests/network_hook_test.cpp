#include "net/ethernet_ipv4.hpp"
#include "net/hook.hpp"
#include "net/nic_probe.hpp"

int main() {
    if (!peregrinus::net::ethernet_ipv4_self_test()) return 1;
    if (!peregrinus::net::hook_self_test()) return 2;
    if (!peregrinus::net::nic::self_test()) return 3;
    return 0;
}
