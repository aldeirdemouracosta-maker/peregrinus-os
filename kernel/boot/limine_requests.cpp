#include "limine_requests.hpp"

namespace peregrinus::boot {
__attribute__((used, section(".limine_requests_start"), aligned(8)))
volatile uint64_t requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile uint64_t base_revision[3] = LIMINE_BASE_REVISION(5);

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_bootloader_info_request bootloader_info_request = { LIMINE_BOOTLOADER_INFO_REQUEST_ID, 0, nullptr };

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_framebuffer_request framebuffer_request = { LIMINE_FRAMEBUFFER_REQUEST_ID, 0, nullptr };

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_memmap_request memmap_request = { LIMINE_MEMMAP_REQUEST_ID, 0, nullptr };

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_hhdm_request hhdm_request = { LIMINE_HHDM_REQUEST_ID, 0, nullptr };

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_rsdp_request rsdp_request = { LIMINE_RSDP_REQUEST_ID, 0, nullptr };

__attribute__((used, section(".limine_requests"), aligned(8)))
volatile limine_module_request module_request = { LIMINE_MODULE_REQUEST_ID, 0, nullptr, 0, nullptr };


__attribute__((used, section(".limine_requests_end"), aligned(8)))
volatile uint64_t requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;
}
