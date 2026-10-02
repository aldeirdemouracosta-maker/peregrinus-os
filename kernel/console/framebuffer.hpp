#pragma once
#include <stdint.h>
struct limine_framebuffer;
namespace peregrinus::framebuffer {
bool init(limine_framebuffer* fb);
bool ready();
void clear(uint32_t rgb);
void status_bars();
uint64_t width();
uint64_t height();
}
