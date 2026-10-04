#pragma once
#include <stdint.h>
namespace peregrinus::keyboard {
// PS/2 scancode set 1 (what firmware leaves enabled through i8042 translation), US layout.
// Pure decoder: feed raw scancodes, get printable ASCII, '\n' (Enter) or '\b' (Backspace);
// 0 means "no character" (key release, modifier, prefix or unmapped key).
class Decoder {
public:
    void reset();
    char feed(uint8_t scancode);
private:
    bool shift_left_ = false, shift_right_ = false, caps_ = false, extended_ = false;
};
}
