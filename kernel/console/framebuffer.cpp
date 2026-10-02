#include "framebuffer.hpp"
#include "../../third_party/limine/limine_min.h"
namespace peregrinus::framebuffer {
static limine_framebuffer* g_fb = nullptr;
bool init(limine_framebuffer* fb) { if (!fb || !fb->address || fb->bpp != 32) return false; g_fb = fb; return true; }
bool ready() { return g_fb != nullptr; }
uint64_t width() { return g_fb ? g_fb->width : 0; }
uint64_t height() { return g_fb ? g_fb->height : 0; }
static uint32_t pixel_value(uint32_t rgb) {
    if (!g_fb) return 0;
    const uint32_t r=(rgb>>16)&0xff, g=(rgb>>8)&0xff, b=rgb&0xff;
    return (r << g_fb->red_mask_shift) | (g << g_fb->green_mask_shift) | (b << g_fb->blue_mask_shift);
}
void clear(uint32_t rgb) {
    if (!g_fb) return;
    const uint32_t p=pixel_value(rgb);
    for (uint64_t y=0; y<g_fb->height; ++y) {
        auto* row = reinterpret_cast<volatile uint32_t*>(reinterpret_cast<uint8_t*>(g_fb->address) + y*g_fb->pitch);
        for (uint64_t x=0; x<g_fb->width; ++x) row[x]=p;
    }
}
static void rect(uint64_t x0,uint64_t y0,uint64_t w,uint64_t h,uint32_t rgb) {
    if(!g_fb) return;
    const uint32_t p=pixel_value(rgb);
    const uint64_t xmax=(x0+w<g_fb->width)?x0+w:g_fb->width, ymax=(y0+h<g_fb->height)?y0+h:g_fb->height;
    for(uint64_t y=y0;y<ymax;++y){ auto* row=reinterpret_cast<volatile uint32_t*>(reinterpret_cast<uint8_t*>(g_fb->address)+y*g_fb->pitch); for(uint64_t x=x0;x<xmax;++x) row[x]=p; }
}
void status_bars() {
    if(!g_fb) return;
    clear(0x101418);
    rect(0,0,g_fb->width,10,0xD9D9D9);
    rect(0,g_fb->height/3,g_fb->width/2,8,0x7FA37D);
    rect(0,g_fb->height/3+18,(g_fb->width*3)/4,8,0x8A9BB5);
    rect(0,g_fb->height/3+36,(g_fb->width*7)/8,8,0xB29A73);
}
}
