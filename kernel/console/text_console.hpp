#pragma once
#include <stdint.h>
namespace peregrinus::text_console {
// Fixed-size text grid mirrored onto the Limine framebuffer, so the boot log is readable on a
// real screen without a serial cable. No heap: the cell buffer is static and bounded.
inline constexpr uint32_t max_cols = 160;  // larger screens get bigger glyphs, not more cells
inline constexpr uint32_t max_rows = 90;
// Returns false (console stays off, serial still works) when there is no usable framebuffer
// or it is too small for a minimal grid.
bool init();
bool ready();
void putc(char c);
uint32_t cols();
uint32_t rows();
char cell(uint32_t row, uint32_t col);  // Latin-1 code point at a cell (tests)
void clear();
}
