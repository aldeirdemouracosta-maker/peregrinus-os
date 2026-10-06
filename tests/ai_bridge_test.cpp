// Serial AI bridge protocol: question frames, answer parsing/sanitizing and the bounded wait.
#include "shell/ai_bridge.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace peregrinus::ai_bridge;

static int fails = 0;
static void check(bool ok, const char* what) { if (!ok) { std::printf("FAIL: %s\n", what); ++fails; } }

// Feeds a whole byte string; returns the first non-pending status (or pending).
static Status feed_all(AnswerParser& p, const std::string& bytes) {
    for (unsigned char c : bytes) { const Status s = p.feed(c); if (s != Status::pending) return s; }
    return Status::pending;
}
static std::string frame(uint32_t seq, const char* kind, const std::string& body) {
    return "\x02PGA1 " + std::to_string(seq) + " " + kind + "\n" + body + "\x03";
}

// Fake I/O for wait(): queued serial bytes and scancodes, a clock that advances on idle().
static std::vector<uint8_t> g_serial, g_keys;
static size_t g_si = 0, g_ki = 0;
static uint64_t g_now = 0;
static int g_idles = 0;
static bool fake_serial(uint8_t& b) { if (g_si >= g_serial.size()) return false; b = g_serial[g_si++]; return true; }
static bool fake_key(uint8_t& b) { if (g_ki >= g_keys.size()) return false; b = g_keys[g_ki++]; return true; }
static uint64_t fake_ticks() { return g_now; }
static void fake_idle() { ++g_idles; ++g_now; }
static void load(const std::string& serial, std::vector<uint8_t> keys = {}) {
    g_serial.assign(serial.begin(), serial.end()); g_keys = keys; g_si = g_ki = 0; g_now = 0; g_idles = 0;
}

int main() {
    // Question frame: header, Latin-1 -> UTF-8, controls removed, capacity respected.
    char buf[64];
    size_t n = encode_question(7, true, "Ol\xE1 \x02mundo\x7F\x85!", buf, sizeof(buf));
    check(std::string(buf, n) == "\x02PGQ1 7 N\nOl\xC3\xA1 mundo!\x03\n", "question frame encoding");
    n = encode_question(4294967295u, false, "x", buf, sizeof(buf));
    check(std::string(buf, n) == "\x02PGQ1 4294967295 C\nx\x03\n", "max sequence number");
    check(encode_question(1, false, "0123456789", buf, 12) == 0, "frame that does not fit is refused");

    AnswerParser p;
    // Plain answer, with log noise before it.
    p.reset(3);
    check(feed_all(p, "peregrinus> lixo\r\n" + frame(3, "OK", "Bras\xC3\xAD" "lia.\nFim")) == Status::answer, "answer recognized");
    check(std::string(p.text()) == "Bras\xC3\xAD" "lia.\nFim" && !p.truncated(), "answer text kept");
    // Host-side error frame.
    p.reset(4);
    check(feed_all(p, frame(4, "ER", "servidor parado")) == Status::host_error && std::string(p.text()) == "servidor parado", "host error frame");
    // A stale answer (other sequence) is skipped, then the right one is taken.
    p.reset(9);
    check(feed_all(p, frame(8, "OK", "velha") + frame(9, "OK", "nova")) == Status::answer && std::string(p.text()) == "nova", "stale answer skipped");
    // Malformed headers end the wait (fail-closed).
    const char* bad[] = {"\x02PGA2 1 OK\nx\x03", "\x02PGA1 OK\nx\x03", "\x02PGA1 1 ok\nx\x03", "\x02PGA1 99999999999 OK\nx\x03",
                         "\x02PGA1 1 OKK\nx\x03", "\x02PGA1 1 OK\x03", "\x02PGA1 1 OK and a very long header\nx\x03",
                         "\x02PGA1 1 OK\nabc\x02"};
    for (const char* b : bad) { p.reset(1); check(feed_all(p, b) == Status::malformed, b + 1); }
    // Sanitizing: ESC/BEL/CR dropped, tab -> space, C1 controls dropped, invalid UTF-8 -> '?'.
    p.reset(5);
    feed_all(p, frame(5, "OK", "a\x1b[2Jb\x07\r\tc\xC2\x9B" "d\xC2\x85" "e\xFF" "f\xC3(\xE0\x80\x80g\xED\xA0\x80h\xF0\x9F\x99\x82i"));
    check(std::string(p.text()) == "a[2Jb c" "de?f?(?g?h\xF0\x9F\x99\x82i", "sanitizing controls and invalid UTF-8");
    // An unfinished multi-byte sequence at the end becomes '?'.
    p.reset(6);
    feed_all(p, frame(6, "OK", "x\xE2\x82"));
    check(std::string(p.text()) == "x?", "truncated UTF-8 at the end");
    // Long answers are cut at max_answer, on a character boundary.
    p.reset(2);
    std::string big; for (int i = 0; i < 1500; ++i) big += "\xC3\xA7";  // 3000 bytes of 'ç'
    check(feed_all(p, frame(2, "OK", big)) == Status::answer, "long answer completes");
    check(p.truncated() && p.length() == max_answer && p.length() % 2 == 0 && std::strlen(p.text()) == p.length(), "truncation at a character boundary");

    // wait(): answer arrives after some idle ticks.
    p.reset(10); load(frame(10, "OK", "ok"));
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 100) == Outcome::answered && std::string(p.text()) == "ok", "wait: answered");
    // Esc cancels even while bytes keep arriving.
    p.reset(11); load("\x02PGA1 11 OK\nmeio", {0x1E, 0x9E, scancode_escape});
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 100) == Outcome::cancelled, "wait: Esc cancels");
    // Silence: the deadline expires after exactly `deadline` idle ticks.
    p.reset(12); load("");
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 50) == Outcome::timed_out && g_idles == 50, "wait: timeout");
    // Endless garbage cannot starve the deadline: the clock is checked between bounded batches.
    p.reset(13); load(std::string(100000, 'x')); g_now = 1000;
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 1000) == Outcome::timed_out && g_si <= 4096, "wait: flood still times out");
    p.reset(14); load(frame(14, "ER", "sem GPU"));
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 10) == Outcome::host_error, "wait: host error");
    p.reset(15); load("\x02PGA1 x OK\n\x03");
    check(wait(p, {fake_serial, fake_key, fake_ticks, fake_idle}, 10) == Outcome::malformed, "wait: malformed");

    // Adversarial: pseudo-random byte soup never overflows and always keeps text NUL-terminated.
    uint64_t x = 0x9E3779B97F4A7C15ull;
    for (int round = 0; round < 2000; ++round) {
        p.reset(static_cast<uint32_t>(round));
        for (int i = 0; i < 600; ++i) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            uint8_t b = static_cast<uint8_t>(x >> 56);
            if ((x & 15) == 0) b = 0x02; else if ((x & 15) == 1) b = 0x03; else if ((x & 15) == 2) b = '\n';
            p.feed(b);
            if (p.length() > max_answer || std::strlen(p.text()) != p.length()) { check(false, "adversarial bounds"); round = 2000; break; }
        }
    }
    if (fails) return 1;
    return 0;
}
