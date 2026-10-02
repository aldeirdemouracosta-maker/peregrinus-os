#pragma once
#include <stddef.h>
#include <stdint.h>

namespace peregrinus::security::sha256 {

struct Context {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t block[64];
    uint32_t block_used;
};

void init(Context& ctx);
void update(Context& ctx, const void* data, size_t bytes);
void final(Context& ctx, uint8_t out[32]);
void digest(const void* data, size_t bytes, uint8_t out[32]);
bool equal(const uint8_t a[32], const uint8_t b[32]);
bool self_test();

}
