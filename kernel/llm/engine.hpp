#pragma once
#include <stddef.h>
#include <stdint.h>
// Local LLM inference engine: a freestanding port of llama2.c's run.c (Andrej Karpathy, MIT),
// the same core used by libclamma (Andy Green, MIT). See THIRD_PARTY_NOTICES.md.
//
// Differences from run.c, all for the kernel:
// - no libc: model and tokenizer are read from memory (Limine modules), buffers come from a
//   fixed-capacity arena reserved once; nothing is freed or grown later;
// - every field of the model/tokenizer files is validated before use (fail-closed);
// - math comes from llm_math (shared with the host reference build for bit-exact tests).
// Supported format: llama2.c "legacy" float32 checkpoints (e.g. stories15M.bin) + tokenizer.bin.
namespace peregrinus::llm {

class Arena {
public:
    Arena(uint8_t* base, size_t size) : base_(base), size_(size) {}
    void* take(size_t bytes, size_t align = 16) {
        const size_t start = (used_ + align - 1) & ~(align - 1);
        if (start > size_ || bytes > size_ - start) return nullptr;
        used_ = start + bytes;
        return base_ + start;
    }
    size_t used() const { return used_; }
private:
    uint8_t* base_; size_t size_; size_t used_ = 0;
};

struct Config { int32_t dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len; };

enum class LoadStatus : uint8_t { ok, model_too_small, bad_config, model_size_mismatch, tokenizer_malformed, out_of_memory };
const char* load_status_name(LoadStatus s);

// Bytes the arena needs for a given model/tokenizer (computed before reserving memory).
size_t arena_bytes_needed(const uint8_t* model, size_t model_size, const uint8_t* tokenizer, size_t tokenizer_size);

using Output = void (*)(const char* utf8);

class Engine {
public:
    LoadStatus load(const uint8_t* model, size_t model_size, const uint8_t* tokenizer, size_t tokenizer_size, Arena& arena);
    bool loaded() const { return loaded_; }
    const Config& config() const { return cfg_; }
    // Generates up to `steps` positions (clamped to seq_len) after the UTF-8 prompt, printing
    // decoded pieces as they are produced. temperature 0 = greedy (deterministic).
    // Returns the number of tokens printed, or -1 if the prompt is too long.
    int generate(const char* prompt, int steps, float temperature, float topp, uint64_t seed, Output out);
private:
    struct Weights { const float *token_embedding_table, *rms_att_weight, *rms_ffn_weight, *wq, *wk, *wv, *wo, *w1, *w2, *w3, *rms_final_weight, *wcls; };
    struct State { float *x, *xb, *xb2, *hb, *hb2, *q, *k, *v, *att, *logits, *key_cache, *value_cache; };
    struct TokenIndex { const char* str; int id; };
    struct ProbIndex { float prob; int index; };

    Config cfg_{};
    Weights w_{};
    State s_{};
    bool loaded_ = false;
    // tokenizer
    char** vocab_ = nullptr; float* vocab_scores_ = nullptr; TokenIndex* sorted_vocab_ = nullptr;
    int vocab_size_ = 0; unsigned max_token_length_ = 0; unsigned char byte_pieces_[512]{};
    char* str_buffer_ = nullptr; int* prompt_tokens_ = nullptr; ProbIndex* probindex_ = nullptr;
    static constexpr int max_prompt_bytes = 512;
    uint64_t rng_state_ = 0;

    float* forward(int token, int pos);
    int str_lookup(const char* str) const;
    void encode(const char* text, int* tokens, int* n_tokens);
    const char* decode(int prev_token, int token) const;
    int sample(float* logits, float temperature, float topp);
};
}
