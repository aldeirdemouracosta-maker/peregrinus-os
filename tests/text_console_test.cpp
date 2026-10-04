// Framebuffer text console against an in-memory 32-bpp framebuffer: glyph pixels, wrapping,
// half-screen scrolling, UTF-8 handling and refusal of unusable framebuffers.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "console/framebuffer.hpp"
#include "console/text_console.hpp"
#include "console/font8x8.hpp"
#include "../third_party/limine/limine_min.h"

using namespace peregrinus;

static std::vector<uint32_t> g_pixels;
static limine_framebuffer make_fb(uint64_t w, uint64_t h) {
    g_pixels.assign(w * h, 0xDEADBEEF);
    limine_framebuffer fb{};
    fb.address = g_pixels.data(); fb.width = w; fb.height = h; fb.pitch = w * 4; fb.bpp = 32;
    fb.red_mask_size = 8; fb.red_mask_shift = 16; fb.green_mask_size = 8; fb.green_mask_shift = 8; fb.blue_mask_size = 8; fb.blue_mask_shift = 0;
    return fb;
}
static void puts_console(const char* s) { while (*s) text_console::putc(*s++); }
static bool row_is(uint32_t r, const char* text) {
    for (uint32_t c = 0; text[c]; ++c) if (text_console::cell(r, c) != text[c]) return false;
    return true;
}

int main() {
    // 640x400 -> scale 1, cells 8x10 -> 80 cols x 40 rows.
    auto fb = make_fb(640, 400);
    if (!framebuffer::init(&fb) || !text_console::init()) { std::puts("FAIL: init"); return 1; }
    if (text_console::cols() != 80 || text_console::rows() != 40) { std::puts("FAIL: grid size"); return 2; }
    if (g_pixels[0] != 0x101418) { std::puts("FAIL: screen not cleared to background"); return 3; }

    puts_console("A");
    // Every pixel of cell (0,0) must match glyph 'A' (bit 0 = leftmost pixel).
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x) {
            const bool on = (font8x8::basic['A'][y] >> x) & 1u;
            if (g_pixels[y * 640 + x] != (on ? 0xD8DEE6u : 0x101418u)) { std::puts("FAIL: glyph pixels"); return 4; }
        }

    // Wrap at the right edge, CR ignored, newline.
    text_console::init();
    for (int i = 0; i < 85; ++i) text_console::putc('x');
    if (text_console::cell(0, 79) != 'x' || text_console::cell(1, 4) != 'x' || text_console::cell(1, 5) != ' ') { std::puts("FAIL: wrap"); return 5; }

    // UTF-8: one '?' per non-ASCII character, continuation bytes skipped.
    text_console::init();
    puts_console("Peregrinus OS \xE2\x80\x94 Purgat\xC3\xB3rio \xC3\xA7\xC3\xA3o\r\n");
    if (!row_is(0, "Peregrinus OS ? Purgat\xF3rio \xE7\xE3o")) { std::puts("FAIL: UTF-8 rendering (Latin-1 accents)"); return 6; }
    // Truncated / invalid sequences show '?' and never desynchronise the following ASCII.
    text_console::init();
    puts_console("a\xC3" "b\xC2\x85" "c\xF0\x9F\x98\x80" "d");
    if (!row_is(0, "a?b?c?d")) { std::puts("FAIL: malformed UTF-8 handling"); return 12; }
    // Backspace erases the previous cell.
    text_console::init();
    puts_console("abc\b\bX");
    if (!row_is(0, "aX ")) { std::puts("FAIL: backspace"); return 13; }

    // Half-screen scroll keeps the newest lines visible and in order.
    text_console::init();
    char line[16];
    for (int i = 0; i < 100; ++i) { std::snprintf(line, sizeof line, "line %d\n", i); puts_console(line); }
    bool found = false;
    for (uint32_t r = 0; r + 1 < text_console::rows(); ++r)
        if (row_is(r, "line 98") && row_is(r + 1, "line 99")) found = true;
    if (!found) { std::puts("FAIL: newest lines not visible after scrolling"); return 7; }

    // Large screens get bigger glyphs (4096 px / (8*4) = 128 cols) and stay within the static grid.
    auto big = make_fb(4096, 2400);
    if (!framebuffer::init(&big) || !text_console::init() || text_console::cols() != 128 || text_console::rows() != 60) { std::puts("FAIL: large screen scaling"); return 8; }
    auto huge = make_fb(16384, 9000);
    if (!framebuffer::init(&huge) || !text_console::init() || text_console::cols() > text_console::max_cols || text_console::rows() > text_console::max_rows) { std::puts("FAIL: grid exceeds static buffer"); return 11; }

    // Unusable framebuffers: console refuses (serial-only), nothing is written.
    auto tiny = make_fb(64, 20);
    if (!framebuffer::init(&tiny) || text_console::init()) { std::puts("FAIL: tiny framebuffer accepted"); return 9; }
    auto bad = make_fb(64, 64); bad.bpp = 16;
    if (framebuffer::init(&bad)) { std::puts("FAIL: 16-bpp framebuffer accepted"); return 10; }
    return 0;
}
