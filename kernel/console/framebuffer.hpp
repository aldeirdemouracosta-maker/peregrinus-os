#pragma once
#include <stdint.h>
struct limine_framebuffer;
namespace peregrinus::framebuffer {
bool init(limine_framebuffer* fb);
bool ready();
void clear(uint32_t rgb);
uint64_t width();
uint64_t height();
// Draws one 8x8 glyph scaled by `scale` into a cell of (8*scale) x cell_height pixels at
// (x0,y0); glyph rows beyond 8*scale are background. Clipped to the framebuffer.
void draw_glyph(uint64_t x0,uint64_t y0,const uint8_t glyph[8],uint32_t scale,uint32_t cell_height,uint32_t fg_rgb,uint32_t bg_rgb);
}
