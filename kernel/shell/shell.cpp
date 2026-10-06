#include "shell.hpp"
#include "holyc.hpp"
namespace peregrinus::shell {
namespace {
bool same(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return *a == 0 && *b == 0; }
void dec(uint64_t v, Output out) {
    char t[21]; int i = 20; t[i] = 0;
    do { t[--i] = static_cast<char>('0' + v % 10); v /= 10; } while (v && i > 0);
    out(t + i);
}
void line(Output out, const char* s) { out(s); out("\n"); }
// User text is stored as Latin-1; the console and serial expect UTF-8.
void out_latin1(Output out, const char* s) {
    for (; *s; ++s) {
        const uint8_t b = static_cast<uint8_t>(*s);
        if (b < 0x80) { const char t[2] = {*s, 0}; out(t); }
        else { const char t[3] = {static_cast<char>(0xC0 | (b >> 6)), static_cast<char>(0x80 | (b & 0x3F)), 0}; out(t); }
    }
}
void yes_no(Output out, const char* label, bool v) { out(label); line(out, v ? "sim" : "não"); }
// First word of the line (lowercased ASCII) into `cmd`; returns false if the word is too long.
// Second word (lowercased ASCII), empty if none.
void second_word(const char* s, char (&arg)[16]) {
    size_t n = 0;
    while (*s == ' ') ++s;
    while (*s && *s != ' ') ++s;
    while (*s == ' ') ++s;
    for (; *s && *s != ' ' && n + 1 < sizeof(arg); ++s) { char c = *s; if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); arg[n++] = c; }
    arg[n] = 0;
}
bool first_word(const char* s, char (&cmd)[16]) {
    while (*s == ' ') ++s;
    size_t n = 0;
    for (; *s && *s != ' '; ++s) {
        if (n + 1 >= sizeof(cmd)) return false;
        char c = *s; if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        cmd[n++] = c;
    }
    cmd[n] = 0;
    return true;
}
}

bool LineEditor::feed(char c, Output echo) {
    if (c == '\r' || c == '\n') { buf_[len_] = 0; if (echo) echo("\n"); return true; }
    if (c == '\b' || c == 0x7F) { if (len_) { --len_; buf_[len_] = 0; if (echo) echo("\b \b"); } return false; }
    const uint8_t b = static_cast<uint8_t>(c);
    if (b < 0x20 || (b > 0x7E && b < 0xA0)) return false;  // control bytes ignored
    if (len_ >= capacity) return false;
    buf_[len_++] = c; buf_[len_] = 0;
    if (echo) {
        if (b < 0x80) { const char s[2] = {c, 0}; echo(s); }
        else { const char s[3] = {static_cast<char>(0xC0 | (b >> 6)), static_cast<char>(0x80 | (b & 0x3F)), 0}; echo(s); }
    }
    return false;
}
bool LineEditor::feed_utf8(char c, Output echo) {
    const uint8_t b = static_cast<uint8_t>(c);
    if (utf8_lead_) {
        const uint8_t lead = utf8_lead_; utf8_lead_ = 0;
        if ((b & 0xC0) == 0x80) return feed(static_cast<char>(((lead & 0x1F) << 6) | (b & 0x3F)), echo);
    }
    if (utf8_skip_) { if ((b & 0xC0) == 0x80) { --utf8_skip_; return false; } utf8_skip_ = 0; }
    if (b == 0xC2 || b == 0xC3) { utf8_lead_ = b; return false; }       // U+0080..U+00FF
    if (b >= 0x80) {                                                     // anything else: dropped
        utf8_skip_ = (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : 0;
        return false;
    }
    return feed(c, echo);
}

const char* prompt() { return "peregrinus> "; }

Action execute(const char* text, const SystemInfo& info, Output out) {
    char cmd[16];
    if (!first_word(text, cmd)) { line(out, "Comando longo demais. Digite 'ajuda'."); return Action::none; }
    if (cmd[0] == 0) return Action::none;
    if (same(cmd, "ajuda") || same(cmd, "help")) {
        line(out, "Comandos:");
        line(out, "  ajuda      esta lista");
        line(out, "  sobre      versão do sistema");
        line(out, "  status     estado de segurança do boot");
        line(out, "  hw         resumo do hardware");
        line(out, "  tempo      tempo desde o boot");
        line(out, "  teclado    mostra ou troca o layout: teclado abnt2 | teclado us");
        line(out, "  hc <código> roda HolyC (subconjunto); 'hc' sozinho abre várias linhas até 'fim'");
        line(out, "  conversa <texto>   IA local (perfil llm-local); 'conversa -g <texto>' = determinístico");
        line(out, "  pergunte <texto>   IA do Linux pela serial (perfil ia-ponte); 'pergunte -n <texto>' = nova conversa");
        line(out, "  limpar     limpa a tela");
        line(out, "  reiniciar  reinicia o computador");
        line(out, "  parar      para o sistema (pode desligar depois)");
        return Action::none;
    }
    if (same(cmd, "sobre") || same(cmd, "about")) {
        out("Peregrinus OS - "); line(out, info.release);
        out("Geração "); dec(info.generation, out); out(", security epoch "); dec(info.epoch, out); out("\n");
        line(out, "Kernel micro x86_64: sem heap, fail-closed, firewall default-deny.");
        return Action::none;
    }
    if (same(cmd, "status")) {
        out("Guard: "); line(out, info.guard_action);
        yes_no(out, "Integridade do .text (verificação de corrupção): ", info.integrity_ok);
        out("Canário de pilha: ativo, semente "); line(out, info.canary_source);
        yes_no(out, "Console na tela: ", info.screen_console);
        yes_no(out, "Teclado PS/2: ", info.keyboard);
        out("Interrupções: "); line(out, info.interrupts ? "ativas (timer PIT, teclado, serial; CPU ociosa em hlt)" : "desligadas (modo polling)");
        return Action::none;
    }
    if (same(cmd, "hw")) {
        out("RAM utilizável: "); dec(info.usable_mib, out); line(out, " MiB");
        out("Funções PCI: "); dec(info.pci_functions, out); out("\n");
        out("CPUs (LAPIC): "); dec(info.local_apics, out); out(", IOAPICs: "); dec(info.io_apics, out); out("\n");
        yes_no(out, "ACPI MADT: ", info.acpi_madt);
        yes_no(out, "ACPI MCFG: ", info.acpi_mcfg);
        return Action::none;
    }
    if (same(cmd, "tempo") || same(cmd, "uptime")) {
        if (!info.interrupts || !info.ticks || info.timer_hz == 0) { line(out, "Sem timer: interrupções desligadas (modo polling)."); return Action::none; }
        const uint64_t t = info.ticks(), s = t / info.timer_hz;
        out("Tempo desde o boot: "); dec(s / 3600, out); out("h "); dec((s / 60) % 60, out); out("min "); dec(s % 60, out);
        out("s ("); dec(t, out); line(out, " ticks)");
        return Action::none;
    }
    if (same(cmd, "teclado") || same(cmd, "keyboard")) {
        char arg[16]; second_word(text, arg);
        if (same(arg, "abnt2") || same(arg, "br")) { line(out, "Teclado: ABNT2 (Brasil)."); return Action::layout_abnt2; }
        if (same(arg, "us")) { line(out, "Teclado: US."); return Action::layout_us; }
        if (arg[0]) { line(out, "Layouts disponíveis: abnt2, us."); return Action::none; }
        out("Teclado atual: "); line(out, info.layout ? info.layout() : "desconhecido");
        return Action::none;
    }
    if (same(cmd, "hc") || same(cmd, "holyc")) {
        const char* p = text; while (*p == ' ') ++p; while (*p && *p != ' ') ++p; while (*p == ' ') ++p;
        if (!*p) { line(out, "HolyC: digite o programa; uma linha 'fim' executa."); return Action::holyc_block; }
        size_t n = 0; while (p[n]) ++n;
        run_holyc(p, n, out);
        return Action::none;
    }
    if (same(cmd, "conversa") || same(cmd, "chat")) {
        if (!info.converse) { line(out, "IA local não incluída nesta build (use o perfil llm-local)."); return Action::none; }
        const char* p = text; while (*p == ' ') ++p; while (*p && *p != ' ') ++p; while (*p == ' ') ++p;
        bool greedy = false;
        if (p[0] == '-' && p[1] == 'g' && (p[2] == ' ' || p[2] == 0)) { greedy = true; p += 2; while (*p == ' ') ++p; }
        info.converse(p, greedy, out);
        return Action::none;
    }
    if (same(cmd, "pergunte") || same(cmd, "ask")) {
        if (!info.ask) { line(out, "Ponte de IA não incluída nesta build (use o perfil ia-ponte)."); return Action::none; }
        const char* p = text; while (*p == ' ') ++p; while (*p && *p != ' ') ++p; while (*p == ' ') ++p;
        bool fresh = false;
        if (p[0] == '-' && p[1] == 'n' && (p[2] == ' ' || p[2] == 0)) { fresh = true; p += 2; while (*p == ' ') ++p; }
        if (!*p) { line(out, "Uso: pergunte <texto>  (ou: pergunte -n <texto> para começar outra conversa)"); return Action::none; }
        info.ask(p, fresh, out);
        return Action::none;
    }
    if (same(cmd, "limpar") || same(cmd, "clear")) return Action::clear_screen;
    if (same(cmd, "reiniciar") || same(cmd, "reboot")) { line(out, "Reiniciando..."); return Action::reboot; }
    if (same(cmd, "parar") || same(cmd, "halt")) { line(out, "Sistema parado. Pode desligar o computador."); return Action::halt; }
    out("Comando desconhecido: "); out_latin1(out, cmd); line(out, ". Digite 'ajuda'.");
    return Action::none;
}

void run_holyc(const char* source, size_t length, Output out) {
    const holyc::Result r = holyc::run(source, length, out);
    if (r.ok) return;
    out("\nerro na linha "); dec(r.line, out); out(": "); line(out, r.error);
}

bool HolycBlock::feed_line(const char* text, Output out) {
    if (!active_) return true;
    if (same(text, "fim") || same(text, "end")) {
        active_ = false;
        if (overflow_) { line(out, "HolyC: programa maior que 2048 bytes; descartado."); return true; }
        run_holyc(buf_, len_, out);
        return true;
    }
    size_t n = 0; while (text[n]) ++n;
    if (len_ + n + 1 > sizeof(buf_)) { overflow_ = true; return false; }
    for (size_t i = 0; i < n; ++i) buf_[len_++] = text[i];
    buf_[len_++] = '\n';
    return false;
}
}
