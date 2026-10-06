#include "framebuffer.hpp"
#include "../../third_party/limine/limine_min.h"
namespace peregrinus::framebuffer {
static limine_framebuffer* g_fb = nullptr;
bool init(limine_framebuffer* fb) { if (!fb || !fb->address || fb->bpp != 32 || fb->pitch < fb->width * 4) return false; g_fb = fb; return true; }
bool ready() { return g_fb != nullptr; }
uint64_t width() { return g_fb ? g_fb->width : 0; }
uint64_t height() { return g_fb ? g_fb->height : 0; }
static uint32_t pixel_value(uint32_t rgb) {
    if (!g_fb) return 0;
    const uint32_t r=(rgb>>16)&0xff, g=(rgb>>8)&0xff, b=rgb&0xff;
    return (r << g_fb->red_mask_shift) | (g << g_fb->green_mask_shift) | (b << g_fb->blue_mask_shift);
}
static volatile uint32_t* row_ptr(uint64_t y) { return reinterpret_cast<volatile uint32_t*>(reinterpret_cast<uint8_t*>(g_fb->address) + y*g_fb->pitch); }
void clear(uint32_t rgb) {
    if (!g_fb) return;
    const uint32_t p=pixel_value(rgb);
    for (uint64_t y=0; y<g_fb->height; ++y) { auto* row=row_ptr(y); for (uint64_t x=0; x<g_fb->width; ++x) row[x]=p; }
}
void draw_glyph(uint64_t x0,uint64_t y0,const uint8_t glyph[8],uint32_t scale,uint32_t cell_height,uint32_t fg_rgb,uint32_t bg_rgb) {
    if (!g_fb || !glyph || scale == 0) return;
    const uint32_t fg=pixel_value(fg_rgb), bg=pixel_value(bg_rgb);
    const uint64_t cell_w=8ull*scale;
    for (uint32_t dy=0; dy<cell_height; ++dy) {
        const uint64_t y=y0+dy; if (y>=g_fb->height) break;
        const uint32_t gy=dy/scale;
        const uint8_t bits=gy<8?glyph[gy]:0;
        auto* row=row_ptr(y);
        for (uint64_t dx=0; dx<cell_w; ++dx) {
            const uint64_t x=x0+dx; if (x>=g_fb->width) break;
            row[x]=((bits>>(dx/scale))&1u)?fg:bg;
        }
    }
}
}
