#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::security::integrity {
inline constexpr uint32_t manifest_version=1;
inline constexpr size_t manifest_bytes=96;

struct Manifest {
    bool valid_magic;
    bool valid_version;
    bool valid_size;
    uint64_t text_size;
    uint8_t text_sha256[32];
    char label[33];
};

struct Report {
    bool manifest_valid;
    bool text_size_match;
    bool digest_match;
    uint64_t text_size;
    uint8_t expected[32];
    uint8_t computed[32];
    char label[33];
    bool trusted() const { return manifest_valid && text_size_match && digest_match; }
};

Manifest parse_manifest(const uint8_t* bytes,size_t size);
Report verify_kernel_text();
bool self_test();
}
