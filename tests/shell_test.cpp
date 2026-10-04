// Keyboard decoder (scancode set 1), bounded line editor and shell commands.
#include <cstdio>
#include <cstring>
#include <string>
#include "input/keyboard.hpp"
#include "shell/shell.hpp"

using namespace peregrinus;
static std::string g_out;
static void sink(const char* s) { g_out += s; }

static std::string type(keyboard::Decoder& d, std::initializer_list<int> codes) {
    std::string r;
    for (int c : codes) { const char ch = d.feed(static_cast<uint8_t>(c)); if (ch) r += ch; }
    return r;
}

int main() {
    keyboard::Decoder d; d.reset();
    // "ajuda" + Enter, with key releases interleaved (release = make | 0x80).
    if (type(d, {0x1E, 0x9E, 0x24, 0xA4, 0x16, 0x96, 0x20, 0xA0, 0x1E, 0x9E, 0x1C, 0x9C}) != "ajuda\n") { std::puts("FAIL: plain typing"); return 1; }
    // Shift held: "A!", released: "a"; Caps Lock inverts letters only.
    if (type(d, {0x2A, 0x1E, 0x02, 0xAA, 0x1E}) != "A!a") { std::puts("FAIL: shift"); return 2; }
    if (type(d, {0x3A, 0xBA, 0x1E, 0x02, 0x3A, 0x1E}) != "A1a") { std::puts("FAIL: caps lock"); return 3; }
    // Extended (E0) keys are ignored except keypad Enter; Backspace -> '\b'.
    if (type(d, {0xE0, 0x48, 0xE0, 0xC8, 0xE0, 0x1C, 0x0E}) != "\n\b") { std::puts("FAIL: extended/backspace"); return 4; }
    // Out-of-table make codes never index outside the map.
    for (int c = 0; c < 256; ++c) (void)d.feed(static_cast<uint8_t>(c));

    shell::LineEditor ed;
    g_out.clear();
    for (char c : std::string("statuz")) ed.feed(c, sink);
    ed.feed('\b', sink); ed.feed('s', sink);
    if (!ed.feed('\n', sink) || std::strcmp(ed.line(), "status") != 0 || g_out != "statuz\b \bs\n") { std::puts("FAIL: line editor"); return 5; }
    ed.clear();
    for (int i = 0; i < 500; ++i) ed.feed('x', nullptr);
    if (ed.length() != shell::LineEditor::capacity) { std::puts("FAIL: line editor bound"); return 6; }
    ed.clear(); ed.feed('\x01', nullptr); ed.feed('\xC3', nullptr);
    if (ed.length() != 0) { std::puts("FAIL: control/non-ASCII bytes accepted"); return 7; }

    const shell::SystemInfo info{"Purgatorio 0.1.1 Admission Gate", 24, 3, "BOOT-PASSIVE (no storage path)", true, "RDRAND", true, true, 254, 6, 1, 1, true, true, true, 100, [] { return uint64_t(372512); }};
    auto run = [&](const char* l) { g_out.clear(); return shell::execute(l, info, sink); };
    if (run("ajuda") != shell::Action::none || g_out.find("reiniciar") == std::string::npos) { std::puts("FAIL: ajuda"); return 8; }
    if (run("  HELP  ") != shell::Action::none || g_out.find("Comandos:") == std::string::npos) { std::puts("FAIL: help alias/case/spaces"); return 9; }
    if (run("sobre") != shell::Action::none || g_out.find("Geração 24, security epoch 3") == std::string::npos) { std::puts("FAIL: sobre"); return 10; }
    if (run("status") != shell::Action::none || g_out.find("Guard: BOOT-PASSIVE") == std::string::npos || g_out.find("semente RDRAND") == std::string::npos) { std::puts("FAIL: status"); return 11; }
    if (run("hw") != shell::Action::none || g_out.find("RAM utilizável: 254 MiB") == std::string::npos) { std::puts("FAIL: hw"); return 12; }
    if (run("tempo") != shell::Action::none || g_out.find("1h 2min 5s (372512 ticks)") == std::string::npos) { std::puts("FAIL: tempo"); return 17; }
    shell::SystemInfo polled = info; polled.interrupts = false;
    g_out.clear(); shell::execute("tempo", polled, sink);
    if (g_out.find("modo polling") == std::string::npos) { std::puts("FAIL: tempo without timer"); return 18; }
    if (run("limpar") != shell::Action::clear_screen || run("reiniciar") != shell::Action::reboot || run("parar") != shell::Action::halt) { std::puts("FAIL: actions"); return 13; }
    if (run("formatar disco") != shell::Action::none || g_out.find("Comando desconhecido: formatar") == std::string::npos) { std::puts("FAIL: unknown"); return 14; }
    if (run("abcdefghijklmnopqrstuvwxyz") != shell::Action::none || g_out.find("longo demais") == std::string::npos) { std::puts("FAIL: long word"); return 15; }
    if (run("") != shell::Action::none || !g_out.empty()) { std::puts("FAIL: empty line"); return 16; }
    return 0;
}
