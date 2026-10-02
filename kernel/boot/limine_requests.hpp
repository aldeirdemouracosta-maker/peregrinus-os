#pragma once
#include "../../third_party/limine/limine_min.h"

namespace peregrinus::boot {
extern volatile uint64_t base_revision[3];
extern volatile limine_bootloader_info_request bootloader_info_request;
extern volatile limine_framebuffer_request framebuffer_request;
extern volatile limine_memmap_request memmap_request;
extern volatile limine_hhdm_request hhdm_request;
extern volatile limine_rsdp_request rsdp_request;
}
