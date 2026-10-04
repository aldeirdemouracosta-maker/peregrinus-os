#include "keyboard.hpp"
namespace peregrinus::keyboard {
namespace {
// Index = make code (0x00..0x39). 0 = unmapped. Characters are Latin-1.
constexpr char us_normal[0x3A] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
constexpr char us_shift[0x3A] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};
// Dead-key markers (Latin-1 accent codes); they never reach the output directly.
constexpr uint8_t ACUTE = 0xB4, GRAVE = 0x60, TILDE = 0x7E, CIRC = 0x5E, DIAER = 0xA8;
constexpr uint8_t sc_backspace = 0x0E, sc_tab = 0x0F, sc_enter = 0x1C, sc_lshift = 0x2A, sc_rshift = 0x36,
                  sc_caps = 0x3A, sc_alt = 0x38, sc_iso_backslash = 0x56, sc_abnt_slash = 0x73;

bool is_dead(uint8_t c) { return c == ACUTE || c == GRAVE || c == TILDE || c == CIRC || c == DIAER; }
// Accented form of `letter` under dead key `dead`, or 0 if none.
char compose(uint8_t dead, char letter) {
    static const char* const vowels = "aeiouAEIOU";
    // Rows: acute, grave, circumflex, diaeresis over "aeiouAEIOU" (Latin-1).
    static const uint8_t table[4][10] = {
        {0xE1, 0xE9, 0xED, 0xF3, 0xFA, 0xC1, 0xC9, 0xCD, 0xD3, 0xDA},
        {0xE0, 0xE8, 0xEC, 0xF2, 0xF9, 0xC0, 0xC8, 0xCC, 0xD2, 0xD9},
        {0xE2, 0xEA, 0xEE, 0xF4, 0xFB, 0xC2, 0xCA, 0xCE, 0xD4, 0xDB},
        {0xE4, 0xEB, 0xEF, 0xF6, 0xFC, 0xC4, 0xCB, 0xCF, 0xD6, 0xDC}};
    if (dead == TILDE) {
        switch (letter) { case 'a': return char(0xE3); case 'o': return char(0xF5); case 'n': return char(0xF1);
                          case 'A': return char(0xC3); case 'O': return char(0xD5); case 'N': return char(0xD1); default: return 0; }
    }
    const int row = dead == ACUTE ? 0 : dead == GRAVE ? 1 : dead == CIRC ? 2 : dead == DIAER ? 3 : -1;
    if (row < 0) return 0;
    for (int i = 0; i < 10; ++i) if (vowels[i] == letter) return static_cast<char>(table[row][i]);
    return 0;
}
// Spacing form of a dead accent (dead key + space, or dead key + non-composable key).
char spacing(uint8_t dead) { return dead == ACUTE ? char(0xB4) : dead == DIAER ? char(0xA8) : static_cast<char>(dead); }
}

const char* layout_name(Layout l) { return l == Layout::abnt2 ? "ABNT2 (Brasil)" : "US"; }

void Decoder::reset() { shift_left_ = shift_right_ = caps_ = extended_ = altgr_ = false; dead_ = 0; }

char Decoder::base_char(uint8_t code, bool shift) const {
    if (layout_ == Layout::abnt2) {
        switch (code) {  // keys whose legends differ from US on ABNT2
            case 0x07: return shift ? char(DIAER) : '6';
            case 0x1A: return shift ? char(GRAVE) : char(ACUTE);
            case 0x1B: return shift ? '{' : '[';
            case 0x27: return shift ? char(0xC7) : char(0xE7);  // Ç ç
            case 0x28: return shift ? char(CIRC) : char(TILDE);
            case 0x29: return shift ? '"' : '\'';
            case 0x2B: return shift ? '}' : ']';
            case 0x35: return shift ? ':' : ';';
            case sc_iso_backslash: return shift ? '|' : '\\';
            case sc_abnt_slash: return shift ? '?' : '/';
            default: break;
        }
    } else if (code == sc_iso_backslash) {
        return shift ? '|' : '\\';
    }
    if (code >= sizeof(us_normal)) return 0;
    return shift ? us_shift[code] : us_normal[code];
}

uint8_t Decoder::feed(uint8_t sc, char out[2]) {
    if (sc == 0xE0) { extended_ = true; return 0; }
    const bool release = (sc & 0x80) != 0;
    const uint8_t code = sc & 0x7F;
    if (extended_) {
        extended_ = false;
        if (code == sc_alt) { altgr_ = !release; return 0; }      // right Alt = AltGr
        if (!release && code == sc_enter) { out[0] = '\n'; return 1; }  // keypad Enter
        return 0;                                                  // arrows, right Ctrl, etc.
    }
    if (code == sc_lshift) { shift_left_ = !release; return 0; }
    if (code == sc_rshift) { shift_right_ = !release; return 0; }
    if (release) return 0;
    if (code == sc_caps) { caps_ = !caps_; return 0; }
    if (code == sc_enter) { dead_ = 0; out[0] = '\n'; return 1; }
    if (code == sc_backspace) { dead_ = 0; out[0] = '\b'; return 1; }
    char c;
    if (layout_ == Layout::abnt2 && altgr_ && (code == 0x10 || code == 0x11)) c = code == 0x10 ? '/' : '?';  // AltGr+Q/W
    else if (code == sc_tab) c = ' ';
    else c = base_char(code, shift_left_ || shift_right_);
    if (c == 0) return 0;
    if (caps_ && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    else if (caps_ && c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    else if (caps_ && static_cast<uint8_t>(c) == 0xE7) c = char(0xC7);
    else if (caps_ && static_cast<uint8_t>(c) == 0xC7) c = char(0xE7);
    const uint8_t uc = static_cast<uint8_t>(c);
    if (layout_ == Layout::abnt2 && is_dead(uc)) {  // on ABNT2 only dead keys produce these codes
        if (dead_) {  // two dead keys in a row: emit the first accent, keep the second pending
            out[0] = spacing(dead_); dead_ = uc; return 1;
        }
        dead_ = uc;
        return 0;
    }
    if (dead_) {
        const uint8_t d = dead_; dead_ = 0;
        if (c == ' ') { out[0] = spacing(d); return 1; }
        if (const char k = compose(d, c)) { out[0] = k; return 1; }
        out[0] = spacing(d); out[1] = c; return 2;
    }
    out[0] = c;
    return 1;
}
}
