#include "shell.hpp"
namespace peregrinus::shell {
namespace {
bool same(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return *a == 0 && *b == 0; }
void dec(uint64_t v, Output out) {
    char t[21]; int i = 20; t[i] = 0;
    do { t[--i] = static_cast<char>('0' + v % 10); v /= 10; } while (v && i > 0);
    out(t + i);
}
void line(Output out, const char* s) { out(s); out("\n"); }
void yes_no(Output out, const char* label, bool v) { out(label); line(out, v ? "sim" : "não"); }
// First word of the line (lowercased ASCII) into `cmd`; returns false if the word is too long.
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
    if (b < 0x20 || b > 0x7E) return false;  // ASCII command language; control bytes ignored
    if (len_ >= capacity) return false;
    buf_[len_++] = c; buf_[len_] = 0;
    if (echo) { const char s[2] = {c, 0}; echo(s); }
    return false;
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
    if (same(cmd, "limpar") || same(cmd, "clear")) return Action::clear_screen;
    if (same(cmd, "reiniciar") || same(cmd, "reboot")) { line(out, "Reiniciando..."); return Action::reboot; }
    if (same(cmd, "parar") || same(cmd, "halt")) { line(out, "Sistema parado. Pode desligar o computador."); return Action::halt; }
    out("Comando desconhecido: "); out(cmd); line(out, ". Digite 'ajuda'.");
    return Action::none;
}
}
