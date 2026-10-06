#include "ai_bridge.hpp"
namespace peregrinus::ai_bridge {
namespace {
constexpr char stx = 0x02, etx = 0x03;

struct Writer {
    char* out; size_t cap; size_t n = 0; bool ok = true;
    void put(char c) { if (n < cap) out[n++] = c; else ok = false; }
    void put(const char* s) { while (*s) put(*s++); }
    void dec(uint32_t v) {
        char t[10]; int i = 0;
        do { t[i++] = static_cast<char>('0' + v % 10); v /= 10; } while (v);
        while (i) put(t[--i]);
    }
};
}

size_t encode_question(uint32_t seq, bool new_conversation, const char* latin1, char* out, size_t cap) {
    Writer w{out, cap};
    w.put(stx); w.put("PGQ1 "); w.dec(seq); w.put(new_conversation ? " N\n" : " C\n");
    for (const char* p = latin1; *p; ++p) {
        const uint8_t b = static_cast<uint8_t>(*p);
        if (b < 0x20 || (b >= 0x7F && b < 0xA0)) continue;  // controls never enter a frame
        if (b < 0x80) w.put(static_cast<char>(b));
        else { w.put(static_cast<char>(0xC0 | (b >> 6))); w.put(static_cast<char>(0x80 | (b & 0x3F))); }
    }
    w.put(etx); w.put('\n');
    return w.ok ? w.n : 0;
}

void AnswerParser::reset(uint32_t expected_seq) {
    expected_ = expected_seq;
    state_ = State::outside;
    header_len_ = 0;
    error_frame_ = truncated_ = false;
    utf8_have_ = utf8_need_ = 0;
    len_ = 0; text_[0] = 0;
}

void AnswerParser::append(const char* s, size_t n) {
    if (truncated_) return;
    if (n > max_answer - len_) { truncated_ = true; return; }  // whole characters only
    for (size_t i = 0; i < n; ++i) text_[len_++] = s[i];
    text_[len_] = 0;
}

void AnswerParser::body_byte(uint8_t b) {
    if (utf8_need_) {
        if ((b & 0xC0) == 0x80) {
            utf8_[utf8_have_++] = b;
            if (utf8_have_ < utf8_need_ + 1) return;
            const uint8_t lead = utf8_[0];
            uint32_t cp = lead & (utf8_need_ == 1 ? 0x1F : utf8_need_ == 2 ? 0x0F : 0x07);
            for (uint8_t i = 1; i < utf8_have_; ++i) cp = (cp << 6) | (utf8_[i] & 0x3F);
            const uint32_t min = utf8_need_ == 1 ? 0x80 : utf8_need_ == 2 ? 0x800 : 0x10000;
            const uint8_t have = utf8_have_;
            utf8_have_ = utf8_need_ = 0;
            if (cp >= 0x80 && cp <= 0x9F) return;                         // C1 controls: dropped
            if (cp < min || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) { append("?", 1); return; }
            append(reinterpret_cast<const char*>(utf8_), have);
            return;
        }
        utf8_have_ = utf8_need_ = 0;  // broken sequence: replaced, then this byte starts afresh
        append("?", 1);
    }
    if (b < 0x80) {
        if (b == '\n') append("\n", 1);
        else if (b == '\t') append(" ", 1);
        else if (b >= 0x20 && b != 0x7F) { const char c = static_cast<char>(b); append(&c, 1); }
        return;
    }
    if (b >= 0xC2 && b <= 0xDF) utf8_need_ = 1;
    else if (b >= 0xE0 && b <= 0xEF) utf8_need_ = 2;
    else if (b >= 0xF0 && b <= 0xF4) utf8_need_ = 3;
    else { append("?", 1); return; }
    utf8_[0] = b; utf8_have_ = 1;
}

Status AnswerParser::finish_header() {
    header_[header_len_] = 0;
    const char* p = header_;
    const char* prefix = "PGA1 ";
    for (; *prefix; ++prefix, ++p) if (*p != *prefix) { state_ = State::outside; return Status::malformed; }
    uint64_t seq = 0; int digits = 0;
    while (*p >= '0' && *p <= '9' && digits < 10) { seq = seq * 10 + static_cast<uint64_t>(*p - '0'); ++p; ++digits; }
    bool ok = digits > 0 && seq <= 0xFFFFFFFFu && *p++ == ' ';
    bool error = false;
    if (ok && p[0] == 'O' && p[1] == 'K' && p[2] == 0) error = false;
    else if (ok && p[0] == 'E' && p[1] == 'R' && p[2] == 0) error = true;
    else ok = false;
    if (!ok) { state_ = State::outside; return Status::malformed; }
    if (seq != expected_) { state_ = State::skip; return Status::pending; }  // stale answer
    error_frame_ = error; truncated_ = false;
    utf8_have_ = utf8_need_ = 0;
    len_ = 0; text_[0] = 0;
    state_ = State::body;
    return Status::pending;
}

Status AnswerParser::feed(uint8_t b) {
    switch (state_) {
    case State::outside:
        if (b == stx) { state_ = State::header; header_len_ = 0; }
        return Status::pending;
    case State::header:
        if (b == '\n') return finish_header();
        if (b == stx || b == etx || header_len_ + 1 >= sizeof(header_)) { state_ = State::outside; return Status::malformed; }
        header_[header_len_++] = static_cast<char>(b);
        return Status::pending;
    case State::body:
        if (b == etx) {
            if (utf8_need_) { utf8_have_ = utf8_need_ = 0; append("?", 1); }
            state_ = State::outside;
            return error_frame_ ? Status::host_error : Status::answer;
        }
        if (b == stx) { state_ = State::outside; return Status::malformed; }
        body_byte(b);
        return Status::pending;
    case State::skip:
        if (b == etx) state_ = State::outside;
        else if (b == stx) { state_ = State::header; header_len_ = 0; }
        return Status::pending;
    }
    return Status::malformed;
}

Outcome wait(AnswerParser& parser, const Io& io, uint64_t deadline) {
    for (;;) {
        bool any = false;
        uint8_t b = 0;
        // Bounded batch, so a host that streams bytes forever cannot starve the deadline check.
        for (int batch = 0; batch < 4096 && io.serial_byte(b); ++batch) {
            any = true;
            switch (parser.feed(b)) {
            case Status::pending: break;
            case Status::answer: return Outcome::answered;
            case Status::host_error: return Outcome::host_error;
            case Status::malformed: return Outcome::malformed;
            }
        }
        uint8_t sc = 0;
        while (io.scancode(sc)) { any = true; if (sc == scancode_escape) return Outcome::cancelled; }
        if (io.ticks() >= deadline) return Outcome::timed_out;
        if (!any) io.idle();
    }
}
}
