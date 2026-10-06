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
uint8_t g_utf8_lead = 0;          // pending 2-byte lead (0xC2/0xC3) whose code point may be drawable

void draw(uint32_t r, uint32_t c) {
    // Cells hold a Latin-1 code point: 0x00-0x7F basic Latin, 0xA0-0xFF Latin-1 supplement.
    const uint8_t ch = static_cast<uint8_t>(g_cells[r][c]);
    const uint8_t* glyph = ch < 0x80 ? font8x8::basic[ch] : ch >= 0xA0 ? font8x8::ext_latin[ch - 0xA0] : font8x8::basic[uint8_t('?')];
    framebuffer::draw_glyph(uint64_t(c) * 8u * g_scale, uint64_t(r) * g_cell_h, glyph, g_scale, g_cell_h, fg_rgb, bg_rgb);
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
void put_visible(char ch) {  // ch: Latin-1 code point (see draw())
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
    g_utf8_lead = 0;
    framebuffer::clear(bg_rgb);
    g_ready = true;
    return true;
}
bool ready() { return g_ready; }
uint32_t cols() { return g_cols; }
uint32_t rows() { return g_rows; }
void clear() {
    if (!g_ready) return;
    for (uint32_t r = 0; r < max_rows; ++r) clear_row(r);
    g_row = g_col = 0;
    framebuffer::clear(bg_rgb);
}
char cell(uint32_t r, uint32_t c) { return (r < max_rows && c < max_cols) ? g_cells[r][c] : 0; }

void putc(char c) {
    if (!g_ready) return;
    const uint8_t b = static_cast<uint8_t>(c);
    // UTF-8: U+00A0..U+00FF (accents, cedilla) are drawn; any other non-ASCII character shows
    // as one '?'. Malformed sequences never index outside the font.
    if (g_utf8_lead) {
        const uint8_t lead = g_utf8_lead; g_utf8_lead = 0;
        if ((b & 0xC0) == 0x80) {
            const uint32_t cp = (uint32_t(lead & 0x1F) << 6) | (b & 0x3F);
            put_visible(cp >= 0xA0 && cp <= 0xFF ? static_cast<char>(cp) : '?');
            return;
        }
        put_visible('?');  // truncated sequence; fall through and handle b normally
    }
    if (g_utf8_continuation) { if ((b & 0xC0) == 0x80) { --g_utf8_continuation; return; } g_utf8_continuation = 0; }
    if (b >= 0x80) {
        if (b == 0xC2 || b == 0xC3) { g_utf8_lead = b; return; }
        if ((b & 0xE0) == 0xC0) g_utf8_continuation = 1;
        else if ((b & 0xF0) == 0xE0) g_utf8_continuation = 2;
        else if ((b & 0xF8) == 0xF0) g_utf8_continuation = 3;
        put_visible('?');
        return;
    }
    if (c == '\r') return;
    if (c == '\n') { newline(); return; }
    if (c == '\b') { if (g_col > 0) { --g_col; g_cells[g_row][g_col] = ' '; draw(g_row, g_col); } return; }
    if (c == '\t') { do put_visible(' '); while (g_col % 4 != 0 && g_col < g_cols); return; }
    put_visible(b >= 0x20 && b < 0x7F ? c : '?');
}
}
