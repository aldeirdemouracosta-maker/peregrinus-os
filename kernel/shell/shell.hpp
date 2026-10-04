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
};
using Output = void (*)(const char*);
enum class Action : uint8_t { none, clear_screen, reboot, halt };

// Bounded line editor: printable characters are echoed and stored up to `capacity`; extra
// characters are dropped (and not echoed). Returns true when Enter completes a line.
class LineEditor {
public:
    static constexpr size_t capacity = 96;
    bool feed(char c, Output echo);
    const char* line() const { return buf_; }
    size_t length() const { return len_; }
    void clear() { len_ = 0; buf_[0] = 0; }
private:
    char buf_[capacity + 1]{};
    size_t len_ = 0;
};

const char* prompt();
Action execute(const char* line, const SystemInfo& info, Output out);
}
