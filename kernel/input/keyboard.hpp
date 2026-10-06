#pragma once
#include <stdint.h>
namespace peregrinus::keyboard {
enum class Layout : uint8_t { us, abnt2 };
const char* layout_name(Layout l);
// PS/2 scancode set 1 (what firmware leaves enabled through i8042 translation).
// Pure decoder: feed raw scancodes, get characters as Latin-1 bytes (ASCII, plus accented
// letters and ç on ABNT2), '\n' for Enter and '\b' for Backspace. A dead key followed by a
// letter that has no accented form yields two characters, hence the 2-byte output.
class Decoder {
public:
    void reset();
    void set_layout(Layout l) { layout_ = l; dead_ = 0; }
    Layout layout() const { return layout_; }
    // Returns how many characters were written to out (0, 1 or 2).
    uint8_t feed(uint8_t scancode, char (&out)[2]);
private:
    Layout layout_ = Layout::abnt2;
    bool shift_left_ = false, shift_right_ = false, caps_ = false, extended_ = false, altgr_ = false;
    uint8_t dead_ = 0;  // pending dead key: Latin-1 code of the accent (´ ` ~ ^ ¨), 0 if none
    char base_char(uint8_t code, bool shift) const;
};
}
