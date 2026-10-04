// libFuzzer harness for every parser that consumes untrusted bytes (network frames, disk
// metadata, firmware tables). Build/run: tests/fuzz.sh (needs clang + libclang-rt).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "net/datapath.hpp"
#include "net/arp.hpp"
#include "net/icmp.hpp"
#include "storage/gpt.hpp"
#include "storage/identify.hpp"
#include "security/quarantine.hpp"
#include "shell/holyc.hpp"
#include "llm/engine.hpp"
#include <peregrinus/recovery_journal.h>

using namespace peregrinus;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static net::Datapath dp;
    static bool init = false;
    if (!init) {
        dp.reset();
        const security::firewall::AllowRuleV4 icmp_in{1, true, security::firewall::Direction::ingress, security::firewall::Protocol::icmp, 0, 0, 0x0a00020fu, 32, 0, 65535, 0, 65535};
        dp.add_allow_rule(icmp_in);
        init = true;
    }
    if (size < 1) return 0;
    const uint8_t selector = data[0];
    ++data; --size;
    const net::e1000::MacAddress mac{{0x52, 0x54, 0, 0x12, 0x34, 0x56}};
    uint8_t out[1514];
    switch (selector % 7) {
        case 0: {
            const auto dir = (selector & 0x80) ? security::firewall::Direction::ingress : security::firewall::Direction::egress;
            dp.inspect(dir, data, size);
            break;
        }
        case 1: {
            net::arp::Packet p{};
            net::arp::parse(data, size, p);
            net::arp::build_reply(p, mac, 0x0a00020fu, out, sizeof out);
            net::icmp::build_echo_reply(data, size, mac, 0x0a00020fu, out, sizeof out);
            break;
        }
        case 2: {
            const auto h = gpt::parse_header(data, size);
            const auto t = gpt::parse_entries(h, data, size);
            gpt::validate_copy(h, t, gpt::CopyKind::primary, 1u << 20, 512);
            gpt::validate_copy(h, t, gpt::CopyKind::backup, 1u << 20, 512);
            break;
        }
        case 3:
            identify::parse(data, size);
            break;
        case 4:
            if (size >= sizeof(peregrinus_recovery_journal)) {
                peregrinus_recovery_journal j;
                std::memcpy(&j, data, sizeof j);
                if (peregrinus_rj_valid(&j)) {
                    peregrinus_recovery_journal k = j;
                    peregrinus_rj_prepare_attempt(&k, peregrinus_rj_choose_slot(&k));
                    peregrinus_rj_mark_success(&k, k.last_slot, k.current_generation, k.current_epoch);
                }
            }
            break;
        case 5:
            holyc::run(reinterpret_cast<const char*>(data), size, [](const char*) {});
            break;
        case 6: {
            // Model + tokenizer split at a fuzzed point; load (and briefly generate) only when the
            // validated layout fits a 4 MiB arena.
            if (size < 4) break;
            const size_t cut = (size_t(data[0]) << 8 | data[1]) % size;
            static uint8_t arena_mem[4 << 20];
            const size_t need = llm::arena_bytes_needed(data, cut, data + cut, size - cut);
            if (!need || need > sizeof(arena_mem)) break;
            llm::Arena arena(arena_mem, sizeof(arena_mem));
            llm::Engine e;
            if (e.load(data, cut, data + cut, size - cut, arena) == llm::LoadStatus::ok)
                e.generate("ab", 4, 0.0f, 0.9f, 1, [](const char*) {});
            break;
        }
    }
    return 0;
}
