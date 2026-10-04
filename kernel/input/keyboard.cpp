#include "keyboard.hpp"
namespace peregrinus::keyboard {
namespace {
// Index = make code (0x00..0x39). Two strings: unshifted and shifted. ' ' entries unmapped
// except 0x39 (space); '\n' and '\b' are produced explicitly below.
constexpr char normal[0x3A] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
constexpr char shifted[0x3A] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};
constexpr uint8_t sc_backspace = 0x0E, sc_tab = 0x0F, sc_enter = 0x1C, sc_lshift = 0x2A, sc_rshift = 0x36, sc_caps = 0x3A;
}
void Decoder::reset() { shift_left_ = shift_right_ = caps_ = extended_ = false; }
char Decoder::feed(uint8_t sc) {
    if (sc == 0xE0) { extended_ = true; return 0; }
    const bool release = (sc & 0x80) != 0;
    const uint8_t code = sc & 0x7F;
    if (extended_) {
        extended_ = false;
        if (!release && code == sc_enter) return '\n';  // keypad Enter
        return 0;                                        // arrows, right Ctrl/Alt, etc.: ignored
    }
    if (code == sc_lshift) { shift_left_ = !release; return 0; }
    if (code == sc_rshift) { shift_right_ = !release; return 0; }
    if (release) return 0;
    if (code == sc_caps) { caps_ = !caps_; return 0; }
    if (code == sc_enter) return '\n';
    if (code == sc_backspace) return '\b';
    if (code == sc_tab) return ' ';
    if (code >= sizeof(normal)) return 0;
    const bool shift = shift_left_ || shift_right_;
    char c = shift ? shifted[code] : normal[code];
    if (caps_ && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    else if (caps_ && c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return c;
}
}
