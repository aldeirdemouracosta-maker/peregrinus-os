#pragma once
#include <stddef.h>
#include <stdint.h>
// Serial AI bridge (ia-ponte profile): the shell sends a question over COM1 to a bridge
// process on the host (scripts/ia-ponte.py), which asks a local LLM server and writes the
// answer back. The kernel only prints the answer; it is never interpreted or executed.
//
// Frames (UTF-8 text, framed by STX/ETX, which the serial log never contains):
//   kernel -> host: "\x02PGQ1 <seq> <N|C>\n" <question> "\x03\n"   (N = new conversation)
//   host -> kernel: "\x02PGA1 <seq> <OK|ER>\n" <text> "\x03"
// Everything outside a frame is ignored. Answers for another sequence number (a cancelled or
// timed-out question) are skipped. A malformed frame ends the wait (fail-closed).
namespace peregrinus::ai_bridge {

inline constexpr size_t max_answer = 2048;              // UTF-8 bytes kept from an answer
inline constexpr uint32_t answer_timeout_seconds = 120;
inline constexpr uint8_t scancode_escape = 0x01;        // PS/2 set 1 make code: cancels the wait

// Builds the question frame from a Latin-1 line (the shell's line editor format). Returns the
// frame length, or 0 if it does not fit in `cap` (nothing usable is written then).
size_t encode_question(uint32_t seq, bool new_conversation, const char* latin1, char* out, size_t cap);

enum class Status : uint8_t { pending, answer, host_error, malformed };

// Byte-at-a-time parser for the host's answer. The kept text is sanitized: control bytes are
// dropped (except '\n'; '\t' becomes a space), C1 controls (U+0080..U+009F) and invalid UTF-8
// become nothing or '?', and text beyond max_answer is cut at a character boundary.
class AnswerParser {
public:
    void reset(uint32_t expected_seq);
    Status feed(uint8_t byte);
    const char* text() const { return text_; }
    size_t length() const { return len_; }
    bool truncated() const { return truncated_; }
private:
    enum class State : uint8_t { outside, header, body, skip };
    void append(const char* s, size_t n);
    void body_byte(uint8_t b);
    Status finish_header();
    uint32_t expected_ = 0;
    State state_ = State::outside;
    char header_[24]{};
    size_t header_len_ = 0;
    bool error_frame_ = false, truncated_ = false;
    uint8_t utf8_[4]{};
    uint8_t utf8_have_ = 0, utf8_need_ = 0;
    char text_[max_answer + 1]{};
    size_t len_ = 0;
};

enum class Outcome : uint8_t { answered, host_error, cancelled, timed_out, malformed };

// Input and time sources for the wait loop (the kernel passes its IRQ rings and tick counter;
// the host test passes fakes). idle() must return within one timer tick.
struct Io {
    bool (*serial_byte)(uint8_t&);
    bool (*scancode)(uint8_t&);
    uint64_t (*ticks)();
    void (*idle)();
};
// Waits for the answer until `deadline` (in ticks). Esc on the keyboard cancels.
Outcome wait(AnswerParser& parser, const Io& io, uint64_t deadline);
}
