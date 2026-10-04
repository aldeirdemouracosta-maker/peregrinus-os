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
    for (int c : codes) { char o[2]; const uint8_t n = d.feed(static_cast<uint8_t>(c), o); r.append(o, n); }
    return r;
}

int main() {
    keyboard::Decoder d; d.reset(); d.set_layout(keyboard::Layout::us);
    // "ajuda" + Enter, with key releases interleaved (release = make | 0x80).
    if (type(d, {0x1E, 0x9E, 0x24, 0xA4, 0x16, 0x96, 0x20, 0xA0, 0x1E, 0x9E, 0x1C, 0x9C}) != "ajuda\n") { std::puts("FAIL: plain typing"); return 1; }
    // Shift held: "A!", released: "a"; Caps Lock inverts letters only.
    if (type(d, {0x2A, 0x1E, 0x02, 0xAA, 0x1E}) != "A!a") { std::puts("FAIL: shift"); return 2; }
    if (type(d, {0x3A, 0xBA, 0x1E, 0x02, 0x3A, 0x1E}) != "A1a") { std::puts("FAIL: caps lock"); return 3; }
    // Extended (E0) keys are ignored except keypad Enter; Backspace -> '\b'.
    if (type(d, {0xE0, 0x48, 0xE0, 0xC8, 0xE0, 0x1C, 0x0E}) != "\n\b") { std::puts("FAIL: extended/backspace"); return 4; }
    // US: ` ~ ^ are ordinary characters, not dead keys.
    if (type(d, {0x29, 0x2A, 0x29, 0x07, 0xAA}) != "`~^") { std::puts("FAIL: US punctuation"); return 19; }
    // Out-of-table make codes never index outside the map, in both layouts.
    for (int c = 0; c < 256; ++c) { char o[2]; (void)d.feed(static_cast<uint8_t>(c), o); }
    d.reset(); d.set_layout(keyboard::Layout::abnt2);
    for (int c = 0; c < 256; ++c) { char o[2]; (void)d.feed(static_cast<uint8_t>(c), o); }
    d.reset();
    // ABNT2: ç key, Shift+ç = Ç, ISO \ key, ABNT2 / key, AltGr+Q = /, AltGr+W = ?.
    if (type(d, {0x27, 0x2A, 0x27, 0xAA, 0x56, 0x73, 0xE0, 0x38, 0x10, 0x11, 0xE0, 0xB8, 0x10}) != "\xE7\xC7\\//?q") { std::puts("FAIL: ABNT2 keys"); return 20; }
    // Dead keys: ´a = á, ~a = ã, ~o = õ, ^e = ê, `a = à (Shift+´), ¨u = ü (Shift+6), ´ + space = ´,
    // ´ + c (no accented form) = ´c.
    if (type(d, {0x1A, 0x1E}) != "\xE1" || type(d, {0x28, 0x1E}) != "\xE3" || type(d, {0x28, 0x18}) != "\xF5") { std::puts("FAIL: acute/tilde"); return 21; }
    if (type(d, {0x2A, 0x28, 0xAA, 0x12}) != "\xEA" || type(d, {0x2A, 0x1A, 0xAA, 0x1E}) != "\xE0" || type(d, {0x2A, 0x07, 0xAA, 0x16}) != "\xFC") { std::puts("FAIL: circumflex/grave/diaeresis"); return 22; }
    if (type(d, {0x1A, 0x39}) != "\xB4" || type(d, {0x1A, 0x2E}) != "\xB4" "c") { std::puts("FAIL: dead key fallbacks"); return 23; }
    if (type(d, {0x2A, 0x1A, 0xAA, 0x2A, 0x1E, 0xAA}) != "\xC0") { std::puts("FAIL: uppercase accent"); return 24; }
    if (std::strcmp(keyboard::layout_name(d.layout()), "ABNT2 (Brasil)") != 0) { std::puts("FAIL: default layout"); return 25; }

    shell::LineEditor ed;
    g_out.clear();
    for (char c : std::string("statuz")) ed.feed(c, sink);
    ed.feed('\b', sink); ed.feed('s', sink);
    if (!ed.feed('\n', sink) || std::strcmp(ed.line(), "status") != 0 || g_out != "statuz\b \bs\n") { std::puts("FAIL: line editor"); return 5; }
    ed.clear();
    for (int i = 0; i < 500; ++i) ed.feed('x', nullptr);
    if (ed.length() != shell::LineEditor::capacity) { std::puts("FAIL: line editor bound"); return 6; }
    ed.clear(); ed.feed('\x01', nullptr); ed.feed('\x85', nullptr);
    if (ed.length() != 0) { std::puts("FAIL: control bytes accepted"); return 7; }
    // Latin-1 from the keyboard is stored as one byte and echoed as UTF-8.
    ed.clear(); g_out.clear(); ed.feed('\xE7', sink); ed.feed('a', sink);
    if (std::strcmp(ed.line(), "\xE7" "a") != 0 || g_out != "\xC3\xA7" "a") { std::puts("FAIL: Latin-1 editing"); return 26; }
    // UTF-8 from the serial line: 2-byte Latin-1 range kept, other code points dropped.
    ed.clear(); g_out.clear();
    for (char c : std::string("a\xC3\xA3o\xE2\x82\xACz\xC3")) ed.feed_utf8(c, sink);
    if (std::strcmp(ed.line(), "a\xE3oz") != 0) { std::puts("FAIL: UTF-8 serial decoding"); return 27; }

    const shell::SystemInfo info{"Purgatorio 0.1.1 Admission Gate", 24, 3, "BOOT-PASSIVE (no storage path)", true, "RDRAND", true, true, 254, 6, 1, 1, true, true, true, 100, [] { return uint64_t(372512); }, [] { return "ABNT2 (Brasil)"; }};
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
    if (run("teclado") != shell::Action::none || g_out.find("Teclado atual: ABNT2") == std::string::npos) { std::puts("FAIL: teclado query"); return 28; }
    if (run("teclado us") != shell::Action::layout_us || run("TECLADO abnt2") != shell::Action::layout_abnt2 || run("teclado xx") != shell::Action::none) { std::puts("FAIL: teclado switch"); return 29; }
    if (run("\xE7\xE3o") != shell::Action::none || g_out.find("Comando desconhecido: \xC3\xA7\xC3\xA3o") == std::string::npos) { std::puts("FAIL: Latin-1 echo in messages"); return 30; }
    if (run("hc \"%d\\n\", 6*7;") != shell::Action::none || g_out != "42\n") { std::printf("FAIL: hc one-liner [%s]\n", g_out.c_str()); return 31; }
    if (run("hc I64 x = 1/0;") != shell::Action::none || g_out.find("erro na linha 1: divisão por zero") == std::string::npos) { std::puts("FAIL: hc error report"); return 32; }
    if (run("hc") != shell::Action::holyc_block) { std::puts("FAIL: hc block start"); return 33; }
    shell::HolycBlock blk; blk.begin(); g_out.clear();
    if (blk.feed_line("I64 i;", sink) || blk.feed_line("for (i=0;i<3;i++) \"%d\", i;", sink) || !blk.feed_line("fim", sink) || blk.active() || g_out != "012") { std::printf("FAIL: hc block [%s]\n", g_out.c_str()); return 34; }
    blk.begin(); g_out.clear();
    for (int i = 0; i < 40; ++i) blk.feed_line("\"0123456789012345678901234567890123456789012345678901234567890\";", sink);
    if (!blk.feed_line("fim", sink) || g_out.find("maior que 2048") == std::string::npos) { std::puts("FAIL: hc block overflow"); return 35; }
    if (run("limpar") != shell::Action::clear_screen || run("reiniciar") != shell::Action::reboot || run("parar") != shell::Action::halt) { std::puts("FAIL: actions"); return 13; }
    if (run("formatar disco") != shell::Action::none || g_out.find("Comando desconhecido: formatar") == std::string::npos) { std::puts("FAIL: unknown"); return 14; }
    if (run("abcdefghijklmnopqrstuvwxyz") != shell::Action::none || g_out.find("longo demais") == std::string::npos) { std::puts("FAIL: long word"); return 15; }
    if (run("") != shell::Action::none || !g_out.empty()) { std::puts("FAIL: empty line"); return 16; }
    return 0;
}
