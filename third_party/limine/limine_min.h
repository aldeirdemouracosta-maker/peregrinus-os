/*
 * Minimal Limine protocol subset for Peregrinus OS Genesis 0.4.
 * Definitions reproduced from the official Limine protocol header (0BSD),
 * limited to requests used by this milestone.
 * Upstream: https://github.com/limine-bootloader/limine-protocol
 */
#pragma once
#include <stdint.h>

#define LIMINE_REQUESTS_START_MARKER { 0xf6b8f4b39de7d1aeULL, 0xfab91a6940fcb9cfULL, 0x785c6ed015d3e316ULL, 0x181e920a7852b9d9ULL }
#define LIMINE_REQUESTS_END_MARKER   { 0xadc0e0531bb10d03ULL, 0x9572709f31764c62ULL }
#define LIMINE_BASE_REVISION(N)      { 0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, (N) }
#define LIMINE_BASE_REVISION_SUPPORTED(VAR) ((VAR)[2] == 0)
#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL

#define LIMINE_BOOTLOADER_INFO_REQUEST_ID { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202fULL, 0x279426fcf5f59740ULL }
struct limine_bootloader_info_response { uint64_t revision; char *name; char *version; };
struct limine_bootloader_info_request { uint64_t id[4]; uint64_t revision; limine_bootloader_info_response *response; };

#define LIMINE_FIRMWARE_TYPE_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x8c2f75d90bef28a8ULL, 0x7045a4688eac00c3ULL }
#define LIMINE_FIRMWARE_TYPE_X86BIOS 0
#define LIMINE_FIRMWARE_TYPE_EFI32 1
#define LIMINE_FIRMWARE_TYPE_EFI64 2
struct limine_firmware_type_response { uint64_t revision; uint64_t firmware_type; };
struct limine_firmware_type_request { uint64_t id[4]; uint64_t revision; limine_firmware_type_response *response; };

#define LIMINE_FRAMEBUFFER_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL }
struct limine_video_mode {
    uint64_t pitch, width, height; uint16_t bpp; uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift, green_mask_size, green_mask_shift, blue_mask_size, blue_mask_shift;
};
struct limine_framebuffer {
    void *address; uint64_t width, height, pitch; uint16_t bpp; uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift, green_mask_size, green_mask_shift, blue_mask_size, blue_mask_shift;
    uint8_t unused[7]; uint64_t edid_size; void *edid; uint64_t mode_count; limine_video_mode **modes;
};
struct limine_framebuffer_response { uint64_t revision; uint64_t framebuffer_count; limine_framebuffer **framebuffers; };
struct limine_framebuffer_request { uint64_t id[4]; uint64_t revision; limine_framebuffer_response *response; };

#define LIMINE_MEMMAP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806fULL, 0xe304acdfc50c3c62ULL }
#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_EXECUTABLE_AND_MODULES 6
#define LIMINE_MEMMAP_FRAMEBUFFER            7
#define LIMINE_MEMMAP_RESERVED_MAPPED        8
struct limine_memmap_entry { uint64_t base; uint64_t length; uint64_t type; };
struct limine_memmap_response { uint64_t revision; uint64_t entry_count; limine_memmap_entry **entries; };
struct limine_memmap_request { uint64_t id[4]; uint64_t revision; limine_memmap_response *response; };


#define LIMINE_HHDM_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL }
struct limine_hhdm_response { uint64_t revision; uint64_t offset; };
struct limine_hhdm_request { uint64_t id[4]; uint64_t revision; limine_hhdm_response *response; };

#define LIMINE_RSDP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43ULL, 0x27637845accdcf3cULL }
struct limine_rsdp_response { uint64_t revision; void *address; };
struct limine_rsdp_request { uint64_t id[4]; uint64_t revision; limine_rsdp_response *response; };

/* Modules (files loaded by Limine next to the kernel), layout as in limine-protocol limine.h. */
#define LIMINE_MODULE_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x3e7e279702be32afULL, 0xca1c4f3bd1280ceeULL }
struct limine_uuid { uint32_t a; uint16_t b; uint16_t c; uint8_t d[8]; };
struct limine_file {
    uint64_t revision; void *address; uint64_t size; char *path; char *string;
    uint32_t media_type; uint32_t unused; uint8_t tftp_ipv4[4]; uint32_t tftp_port;
    uint32_t partition_index; uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid, gpt_part_uuid, part_uuid;
};
struct limine_module_response { uint64_t revision; uint64_t module_count; limine_file **modules; };
struct limine_module_request { uint64_t id[4]; uint64_t revision; limine_module_response *response; uint64_t internal_module_count; void *internal_modules; };

#define LIMINE_EFI_SYSTEM_TABLE_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x5ceba5163eaaf6d6ULL, 0x0a6981610cf65fccULL }
struct limine_efi_system_table_response { uint64_t revision; void *address; };
struct limine_efi_system_table_request { uint64_t id[4]; uint64_t revision; limine_efi_system_table_response *response; };

#define LIMINE_EFI_MEMMAP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x7df62a431d6872d5ULL, 0xa4fcdfb3e57306c8ULL }
struct limine_efi_memmap_response { uint64_t revision; void *memmap; uint64_t memmap_size; uint64_t desc_size; uint64_t desc_version; };
struct limine_efi_memmap_request { uint64_t id[4]; uint64_t revision; limine_efi_memmap_response *response; };
