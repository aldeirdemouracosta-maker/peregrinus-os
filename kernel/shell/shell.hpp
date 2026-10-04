#pragma once
#include <stddef.h>
#include <stdint.h>
namespace peregrinus::shell {
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
};
using Output = void (*)(const char*);
enum class Action : uint8_t { none, clear_screen, reboot, halt, layout_us, layout_abnt2 };

// Bounded line editor. The line is stored as Latin-1 (ASCII + U+00A0..U+00FF, so Portuguese
// text fits in one byte per character) and echoed as UTF-8. Characters beyond `capacity` are
// dropped (and not echoed). feed()/feed_utf8() return true when Enter completes a line.
class LineEditor {
public:
    static constexpr size_t capacity = 96;
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
}
