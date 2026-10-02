#pragma once
#include <stdint.h>
#include <stddef.h>
#include <peregrinus/release_profile.h>

#define PEREGRINUS_BOOT_REQUEST_VERSION 1u
#ifndef PEREGRINUS_MIN_SECURITY_EPOCH
#define PEREGRINUS_MIN_SECURITY_EPOCH PEREGRINUS_RELEASE_MIN_SECURITY_EPOCH
#endif
#define PEREGRINUS_REQUEST_MAGIC_0 'P'
#define PEREGRINUS_REQUEST_MAGIC_1 'G'
#define PEREGRINUS_REQUEST_MAGIC_2 'R'
#define PEREGRINUS_REQUEST_MAGIC_3 'R'
#define PEREGRINUS_REQUEST_MAGIC_4 'E'
#define PEREGRINUS_REQUEST_MAGIC_5 'Q'
#define PEREGRINUS_REQUEST_MAGIC_6 '0'
#define PEREGRINUS_REQUEST_MAGIC_7 '1'

enum peregrinus_boot_choice : uint8_t {
    PEREGRINUS_BOOT_CURRENT = 0,
    PEREGRINUS_BOOT_LKG = 1
};

#pragma pack(push, 1)
struct peregrinus_boot_request {
    uint8_t magic[8];
    uint32_t version;
    uint8_t choice;
    uint8_t reserved[3];
    uint64_t requested_security_epoch;
    uint64_t generation_hint;
    uint32_t crc32;
};
#pragma pack(pop)

static inline uint32_t peregrinus_crc32(const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

static inline int peregrinus_request_magic_ok(const struct peregrinus_boot_request* r) {
    const uint8_t m[8] = {
        PEREGRINUS_REQUEST_MAGIC_0, PEREGRINUS_REQUEST_MAGIC_1,
        PEREGRINUS_REQUEST_MAGIC_2, PEREGRINUS_REQUEST_MAGIC_3,
        PEREGRINUS_REQUEST_MAGIC_4, PEREGRINUS_REQUEST_MAGIC_5,
        PEREGRINUS_REQUEST_MAGIC_6, PEREGRINUS_REQUEST_MAGIC_7
    };
    for (unsigned i = 0; i < 8; ++i) if (r->magic[i] != m[i]) return 0;
    return 1;
}

static inline uint32_t peregrinus_request_crc(const struct peregrinus_boot_request* r) {
    struct peregrinus_boot_request t = *r;
    t.crc32 = 0;
    return peregrinus_crc32(&t, sizeof(t));
}

static inline int peregrinus_request_valid(const struct peregrinus_boot_request* r) {
    if (!r || !peregrinus_request_magic_ok(r)) return 0;
    if (r->version != PEREGRINUS_BOOT_REQUEST_VERSION) return 0;
    if (r->choice != PEREGRINUS_BOOT_CURRENT && r->choice != PEREGRINUS_BOOT_LKG) return 0;
    if (r->requested_security_epoch < PEREGRINUS_MIN_SECURITY_EPOCH) return 0;
    return peregrinus_request_crc(r) == r->crc32;
}

static inline int peregrinus_request_allows_lkg_floor(const struct peregrinus_boot_request* r,
                                                       uint64_t current_epoch,
                                                       uint64_t lkg_epoch,
                                                       uint64_t active_min_epoch) {
    if (!peregrinus_request_valid(r)) return 0;
    if (r->choice != PEREGRINUS_BOOT_LKG) return 0;
    if (active_min_epoch < PEREGRINUS_MIN_SECURITY_EPOCH) return 0;
    if (current_epoch < active_min_epoch || lkg_epoch < active_min_epoch) return 0;
    return r->requested_security_epoch == lkg_epoch;
}

static inline int peregrinus_request_allows_lkg(const struct peregrinus_boot_request* r,
                                                 uint64_t current_epoch,
                                                 uint64_t lkg_epoch) {
    return peregrinus_request_allows_lkg_floor(r,current_epoch,lkg_epoch,PEREGRINUS_MIN_SECURITY_EPOCH);
}
