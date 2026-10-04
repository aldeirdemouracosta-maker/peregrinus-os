#include "text_console.hpp"
#include "framebuffer.hpp"
#include "font8x8.hpp"

namespace peregrinus::text_console {
namespace {
constexpr uint32_t fg_rgb = 0xD8DEE6, bg_rgb = 0x101418;
char g_cells[max_rows][max_cols];
uint32_t g_cols = 0, g_rows = 0, g_row = 0, g_col = 0, g_scale = 1, g_cell_h = 10;
bool g_ready = false;
uint8_t g_utf8_continuation = 0;  // bytes still to skip in the current UTF-8 sequence

void draw(uint32_t r, uint32_t c) {
    const uint8_t ch = static_cast<uint8_t>(g_cells[r][c]);
    framebuffer::draw_glyph(uint64_t(c) * 8u * g_scale, uint64_t(r) * g_cell_h, font8x8::basic[ch < 128 ? ch : '?'], g_scale, g_cell_h, fg_rgb, bg_rgb);
}
void clear_row(uint32_t r) { for (uint32_t c = 0; c < max_cols; ++c) g_cells[r][c] = ' '; }
// Scroll by half a screen at once: one full redraw per rows/2 lines keeps framebuffer
// traffic bounded (no framebuffer reads, no per-line full redraw).
void scroll() {
    const uint32_t shift = g_rows / 2 ? g_rows / 2 : 1;
    for (uint32_t r = 0; r + shift < g_rows; ++r)
        for (uint32_t c = 0; c < g_cols; ++c) g_cells[r][c] = g_cells[r + shift][c];
    for (uint32_t r = g_rows - shift; r < g_rows; ++r) clear_row(r);
    for (uint32_t r = 0; r < g_rows; ++r)
        for (uint32_t c = 0; c < g_cols; ++c) draw(r, c);
    g_row = g_rows - shift;
}
void newline() { g_col = 0; if (++g_row >= g_rows) scroll(); }
void put_visible(char ch) {
    if (g_col >= g_cols) newline();
    g_cells[g_row][g_col] = ch;
    draw(g_row, g_col);
    ++g_col;
}
}

bool init() {
    g_ready = false;
    if (!framebuffer::ready()) return false;
    const uint64_t w = framebuffer::width(), h = framebuffer::height();
    g_scale = (w >= 1024 && h >= 600) ? 2 : 1;
    while (g_scale < 4 && (w / (8u * g_scale) > max_cols || h / (10u * g_scale) > max_rows)) ++g_scale;
    g_cell_h = 10 * g_scale;  // 8 glyph rows + 2 rows of line spacing
    const uint64_t cols = w / (8u * g_scale), rows = h / g_cell_h;
    if (cols < 16 || rows < 4) return false;
    g_cols = static_cast<uint32_t>(cols < max_cols ? cols : max_cols);
    g_rows = static_cast<uint32_t>(rows < max_rows ? rows : max_rows);
    for (uint32_t r = 0; r < max_rows; ++r) clear_row(r);
    g_row = g_col = 0;
    g_utf8_continuation = 0;
    framebuffer::clear(bg_rgb);
    g_ready = true;
    return true;
}
bool ready() { return g_ready; }
uint32_t cols() { return g_cols; }
uint32_t rows() { return g_rows; }
char cell(uint32_t r, uint32_t c) { return (r < max_rows && c < max_cols) ? g_cells[r][c] : 0; }

void putc(char c) {
    if (!g_ready) return;
    const uint8_t b = static_cast<uint8_t>(c);
    // UTF-8: the font only covers ASCII; show one '?' per non-ASCII character.
    if (g_utf8_continuation) { if ((b & 0xC0) == 0x80) { --g_utf8_continuation; return; } g_utf8_continuation = 0; }
    if (b >= 0x80) {
        if ((b & 0xE0) == 0xC0) g_utf8_continuation = 1;
        else if ((b & 0xF0) == 0xE0) g_utf8_continuation = 2;
        else if ((b & 0xF8) == 0xF0) g_utf8_continuation = 3;
        put_visible('?');
        return;
    }
    if (c == '\r') return;
    if (c == '\n') { newline(); return; }
    if (c == '\t') { do put_visible(' '); while (g_col % 4 != 0 && g_col < g_cols); return; }
    put_visible(b >= 0x20 && b < 0x7F ? c : '?');
}
}
