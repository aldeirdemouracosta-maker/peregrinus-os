#include "security/firewall.hpp"

using namespace peregrinus::security::firewall;

int main() {
    if (!self_test()) return 1;

    Engine e;
    e.reset();
    AllowRuleV4 dns{
        7, true, Direction::egress, Protocol::udp,
        ipv4(192, 168, 1, 0), 24, ipv4(1, 1, 1, 1), 32,
        1024, 65535, 53, 53
    };
    if (!e.add_allow_rule(dns)) return 2;

    PacketV4 ok{Direction::egress, Protocol::udp, ipv4(192, 168, 1, 50), ipv4(1, 1, 1, 1), 53000, 53, false, 0, 0, 0};
    PacketV4 inbound{Direction::ingress, Protocol::udp, ipv4(1, 1, 1, 1), ipv4(192, 168, 1, 50), 53, 53000, false, 0, 0, 0};
    if (e.evaluate(ok).verdict != Verdict::allow) return 3;
    if (e.evaluate(inbound).verdict != Verdict::deny) return 4;
    return 0;
}
