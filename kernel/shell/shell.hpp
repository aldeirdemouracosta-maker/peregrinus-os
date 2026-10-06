#pragma once
#include <stddef.h>
#include <stdint.h>
namespace peregrinus::shell {
using Output = void (*)(const char*);
// Read-only facts the shell can report; filled once by kmain. The shell never changes system
// state except through the returned Action (clear screen, reboot, halt).
struct SystemInfo {
    const char* release;
    uint64_t generation, epoch;
    const char* guard_action;
    bool integrity_ok;
    const char* canary_source;
    bool screen_console, keyboard;
    uint64_t usable_mib, pci_functions, local_apics, io_apics;
    bool acpi_madt, acpi_mcfg;
    bool interrupts;               // PIC/PIT live (false: polling fallback)
    uint32_t timer_hz;
    uint64_t (*ticks)();           // live tick counter (may be null)
    const char* (*layout)();       // current keyboard layout name (may be null)
    // Local LLM (llm-local profile only; null otherwise). prompt is Latin-1.
    void (*converse)(const char* prompt, bool greedy, Output out);
    // Serial AI bridge to an LLM on the host (ia-ponte profile only; null otherwise).
    // question is Latin-1; the answer is only printed, never executed.
    void (*ask)(const char* question, bool new_conversation, Output out);
};
enum class Action : uint8_t { none, clear_screen, reboot, halt, layout_us, layout_abnt2, holyc_block };

// Bounded line editor. The line is stored as Latin-1 (ASCII + U+00A0..U+00FF, so Portuguese
// text fits in one byte per character) and echoed as UTF-8. Characters beyond `capacity` are
// dropped (and not echoed). feed()/feed_utf8() return true when Enter completes a line.
class LineEditor {
public:
    static constexpr size_t capacity = 160;
    bool feed(char latin1, Output echo);        // keyboard path (Latin-1)
    bool feed_utf8(char byte, Output echo);     // serial path (UTF-8 byte stream)
    const char* line() const { return buf_; }
    size_t length() const { return len_; }
    void clear() { len_ = 0; buf_[0] = 0; }
private:
    char buf_[capacity + 1]{};
    size_t len_ = 0;
    uint8_t utf8_lead_ = 0, utf8_skip_ = 0;
};

const char* prompt();
Action execute(const char* line, const SystemInfo& info, Output out);

// Multi-line HolyC entry (`hc` alone): lines accumulate in a fixed buffer until a line `fim`,
// then the program runs. Overflow fails closed (the block is discarded with an error).
class HolycBlock {
public:
    bool active() const { return active_; }
    void begin() { active_ = true; len_ = 0; overflow_ = false; }
    // Returns true when the block ended (and was run or rejected).
    bool feed_line(const char* line, Output out);
    static const char* prompt() { return "hc> "; }
private:
    char buf_[2048];
    size_t len_ = 0;
    bool active_ = false, overflow_ = false;
};
// Runs a HolyC program and reports errors as "erro na linha N: ...".
void run_holyc(const char* source, size_t length, Output out);
}
