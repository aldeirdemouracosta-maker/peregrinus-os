// Port of llama2.c run.c (https://github.com/karpathy/llama2.c, MIT License,
// Copyright (c) 2023 Andrej Karpathy), as also used by libclamma
// (https://github.com/warmcat/libclamma, MIT License, Copyright (C) 2023 Andy Green).
// The forward pass, tokenizer and sampler follow run.c statement by statement so that results
// are bit-identical to the reference (see tests/llm.sh). Kernel adaptations are marked.
#include "engine.hpp"
#include "llm_math.hpp"

namespace peregrinus::llm {
namespace {
void copy_bytes(void* d, const void* s, size_t n) { auto* a = static_cast<uint8_t*>(d); auto* b = static_cast<const uint8_t*>(s); for (size_t i = 0; i < n; ++i) a[i] = b[i]; }
void zero_floats(float* p, size_t n) { for (size_t i = 0; i < n; ++i) p[i] = 0.0f; }
int32_t rd32(const uint8_t* p) { int32_t v; copy_bytes(&v, p, 4); return v; }
// strcmp semantics (unsigned bytes), as used by run.c's qsort/bsearch comparator.
int compare_str(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return static_cast<int>(static_cast<unsigned char>(*a)) - static_cast<int>(static_cast<unsigned char>(*b));
}
size_t str_len(const char* s) { size_t n = 0; while (s[n]) ++n; return n; }

void rmsnorm(float* o, const float* x, const float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) ss += x[j] * x[j];
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / pg_sqrtf(ss);
    for (int j = 0; j < size; j++) o[j] = weight[j] * (ss * x[j]);
}
void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) if (x[i] > max_val) max_val = x[i];
    float sum = 0.0f;
    for (int i = 0; i < size; i++) { x[i] = pg_expf(x[i] - max_val); sum += x[i]; }
    for (int i = 0; i < size; i++) x[i] /= sum;
}
void matmul(float* xout, const float* x, const float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) val += w[i * n + j] * x[j];
        xout[i] = val;
    }
}

// Validated geometry of a legacy float32 checkpoint.
struct Geometry { Config cfg; bool shared; uint64_t weight_floats; };
bool geometry(const uint8_t* model, size_t size, Geometry& g, LoadStatus& why) {
    if (!model || size < sizeof(Config)) { why = LoadStatus::model_too_small; return false; }
    Config c;
    c.dim = rd32(model); c.hidden_dim = rd32(model + 4); c.n_layers = rd32(model + 8); c.n_heads = rd32(model + 12);
    c.n_kv_heads = rd32(model + 16); c.vocab_size = rd32(model + 20); c.seq_len = rd32(model + 24);
    g.shared = c.vocab_size > 0;
    if (c.vocab_size == INT32_MIN) { why = LoadStatus::bad_config; return false; }
    c.vocab_size = c.vocab_size < 0 ? -c.vocab_size : c.vocab_size;
    // Kernel adaptation: bounds on every dimension before any pointer arithmetic.
    const bool ok = c.dim > 0 && c.dim <= 8192 && c.hidden_dim > 0 && c.hidden_dim <= 32768 &&
                    c.n_layers > 0 && c.n_layers <= 64 && c.n_heads > 0 && c.n_heads <= c.dim && c.dim % c.n_heads == 0 &&
                    (c.dim / c.n_heads) % 2 == 0 && c.n_kv_heads > 0 && c.n_kv_heads <= c.n_heads && c.n_heads % c.n_kv_heads == 0 &&
                    c.vocab_size >= 259 && c.vocab_size <= 128000 && c.seq_len > 0 && c.seq_len <= 4096;
    if (!ok) { why = LoadStatus::bad_config; return false; }
    const uint64_t dim = uint64_t(c.dim), L = uint64_t(c.n_layers), hs = dim / uint64_t(c.n_heads);
    const uint64_t kv_dim = dim * uint64_t(c.n_kv_heads) / uint64_t(c.n_heads), hid = uint64_t(c.hidden_dim);
    uint64_t f = uint64_t(c.vocab_size) * dim;          // token embedding
    f += L * dim;                                         // rms_att
    f += L * dim * dim + 2 * L * dim * kv_dim + L * dim * dim;  // wq wk wv wo
    f += L * dim;                                         // rms_ffn
    f += 3 * L * dim * hid;                               // w1 w2 w3
    f += dim;                                             // rms_final
    f += uint64_t(c.seq_len) * hs;                        // legacy freq_cis_real + imag
    if (!g.shared) f += uint64_t(c.vocab_size) * dim;     // wcls
    if (f > (uint64_t(size) - sizeof(Config)) / 4) { why = LoadStatus::model_size_mismatch; return false; }
    g.cfg = c; g.weight_floats = f;
    return true;
}
// Validates tokenizer.bin and reports total string bytes (including terminators).
bool tokenizer_layout(const uint8_t* t, size_t size, int vocab, unsigned& max_len, size_t& string_bytes) {
    if (!t || size < 4) return false;
    const int32_t m = rd32(t);
    if (m <= 0 || m > 1024) return false;
    max_len = unsigned(m);
    size_t off = 4; string_bytes = 0;
    for (int i = 0; i < vocab; i++) {
        if (size - off < 8) return false;
        const int32_t len = rd32(t + off + 4);
        if (len < 0 || len > m || size - off - 8 < size_t(len)) return false;
        off += 8 + size_t(len);
        string_bytes += size_t(len) + 1;
    }
    return true;
}
size_t align16(size_t n) { return (n + 15) & ~size_t(15); }
}

const char* load_status_name(LoadStatus s) {
    switch (s) {
        case LoadStatus::ok: return "OK";
        case LoadStatus::model_too_small: return "MODEL-TOO-SMALL";
        case LoadStatus::bad_config: return "BAD-CONFIG";
        case LoadStatus::model_size_mismatch: return "MODEL-SIZE-MISMATCH";
        case LoadStatus::tokenizer_malformed: return "TOKENIZER-MALFORMED";
        case LoadStatus::out_of_memory: return "OUT-OF-MEMORY";
    }
    return "UNKNOWN";
}

size_t arena_bytes_needed(const uint8_t* model, size_t model_size, const uint8_t* tok, size_t tok_size) {
    Geometry g; LoadStatus why;
    if (!geometry(model, model_size, g, why)) return 0;
    unsigned max_len = 0; size_t strings = 0;
    if (!tokenizer_layout(tok, tok_size, g.cfg.vocab_size, max_len, strings)) return 0;
    const size_t V = size_t(g.cfg.vocab_size), dim = size_t(g.cfg.dim), hid = size_t(g.cfg.hidden_dim);
    const size_t kv_dim = dim * size_t(g.cfg.n_kv_heads) / size_t(g.cfg.n_heads);
    size_t n = 0;
    n += align16(V * sizeof(char*)) + align16(V * sizeof(float)) + align16(strings) + align16(V * 16);
    n += align16(max_len * 2 + 3) + align16((512 + 3) * sizeof(int)) + align16(V * 8);
    n += 3 * align16(dim * 4) + 2 * align16(hid * 4) + align16(dim * 4) + align16(size_t(g.cfg.n_heads) * size_t(g.cfg.seq_len) * 4) + align16(V * 4);
    n += 2 * align16(size_t(g.cfg.n_layers) * size_t(g.cfg.seq_len) * kv_dim * 4);
    return n + 64;
}

LoadStatus Engine::load(const uint8_t* model, size_t model_size, const uint8_t* tok, size_t tok_size, Arena& arena) {
    loaded_ = false;
    Geometry g; LoadStatus why = LoadStatus::ok;
    if (!geometry(model, model_size, g, why)) return why;
    unsigned max_len = 0; size_t strings = 0;
    if (!tokenizer_layout(tok, tok_size, g.cfg.vocab_size, max_len, strings)) return LoadStatus::tokenizer_malformed;
    cfg_ = g.cfg;
    const Config* p = &cfg_;
    // memory_map_weights (run.c), over the module memory.
    const int head_size = p->dim / p->n_heads;
    const unsigned long long n_layers = p->n_layers;
    const float* ptr = reinterpret_cast<const float*>(model + sizeof(Config));
    w_.token_embedding_table = ptr; ptr += p->vocab_size * p->dim;
    w_.rms_att_weight = ptr; ptr += n_layers * p->dim;
    w_.wq = ptr; ptr += n_layers * p->dim * (p->n_heads * head_size);
    w_.wk = ptr; ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w_.wv = ptr; ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w_.wo = ptr; ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w_.rms_ffn_weight = ptr; ptr += n_layers * p->dim;
    w_.w1 = ptr; ptr += n_layers * p->dim * p->hidden_dim;
    w_.w2 = ptr; ptr += n_layers * p->hidden_dim * p->dim;
    w_.w3 = ptr; ptr += n_layers * p->dim * p->hidden_dim;
    w_.rms_final_weight = ptr; ptr += p->dim;
    ptr += p->seq_len * head_size / 2;
    ptr += p->seq_len * head_size / 2;
    w_.wcls = g.shared ? w_.token_embedding_table : ptr;

    // malloc_run_state (run.c) -> fixed arena; buffers zeroed like calloc.
    const int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    auto floats = [&](size_t n) { float* f = static_cast<float*>(arena.take(n * sizeof(float))); if (f) zero_floats(f, n); return f; };
    s_.x = floats(p->dim); s_.xb = floats(p->dim); s_.xb2 = floats(p->dim);
    s_.hb = floats(p->hidden_dim); s_.hb2 = floats(p->hidden_dim); s_.q = floats(p->dim);
    s_.key_cache = floats(size_t(p->n_layers) * p->seq_len * kv_dim);
    s_.value_cache = floats(size_t(p->n_layers) * p->seq_len * kv_dim);
    s_.att = floats(size_t(p->n_heads) * p->seq_len);
    s_.logits = floats(p->vocab_size);
    if (!s_.x || !s_.xb || !s_.xb2 || !s_.hb || !s_.hb2 || !s_.q || !s_.key_cache || !s_.value_cache || !s_.att || !s_.logits) return LoadStatus::out_of_memory;

    // build_tokenizer (run.c), copying strings out of the module so they are NUL-terminated.
    vocab_size_ = p->vocab_size;
    max_token_length_ = max_len;
    vocab_ = static_cast<char**>(arena.take(size_t(vocab_size_) * sizeof(char*)));
    vocab_scores_ = static_cast<float*>(arena.take(size_t(vocab_size_) * sizeof(float)));
    char* strings_area = static_cast<char*>(arena.take(strings, 1));
    sorted_vocab_ = static_cast<TokenIndex*>(arena.take(size_t(vocab_size_) * sizeof(TokenIndex)));
    str_buffer_ = static_cast<char*>(arena.take(max_token_length_ * 2 + 1 + 2, 1));
    prompt_tokens_ = static_cast<int*>(arena.take((max_prompt_bytes + 3) * sizeof(int)));
    probindex_ = static_cast<ProbIndex*>(arena.take(size_t(vocab_size_) * sizeof(ProbIndex)));
    if (!vocab_ || !vocab_scores_ || !strings_area || !sorted_vocab_ || !str_buffer_ || !prompt_tokens_ || !probindex_) return LoadStatus::out_of_memory;
    for (int i = 0; i < 256; i++) { byte_pieces_[i * 2] = static_cast<unsigned char>(i); byte_pieces_[i * 2 + 1] = '\0'; }
    size_t off = 4;
    for (int i = 0; i < vocab_size_; i++) {
        copy_bytes(&vocab_scores_[i], tok + off, 4);
        const int32_t len = rd32(tok + off + 4);
        off += 8;
        vocab_[i] = strings_area;
        copy_bytes(strings_area, tok + off, size_t(len));
        strings_area[len] = '\0';
        strings_area += len + 1;
        off += size_t(len);
    }
    // Sorted index for str_lookup. run.c uses qsort; heapsort gives the same order whenever the
    // vocabulary has unique strings (rejected below otherwise: duplicates make lookups ambiguous).
    for (int i = 0; i < vocab_size_; i++) { sorted_vocab_[i].str = vocab_[i]; sorted_vocab_[i].id = i; }
    auto less = [](const TokenIndex& a, const TokenIndex& b) { return compare_str(a.str, b.str) < 0; };
    auto sift = [&](int start, int end) {
        int root = start;
        while (2 * root + 1 <= end) {
            int child = 2 * root + 1, swap = root;
            if (less(sorted_vocab_[swap], sorted_vocab_[child])) swap = child;
            if (child + 1 <= end && less(sorted_vocab_[swap], sorted_vocab_[child + 1])) swap = child + 1;
            if (swap == root) return;
            const TokenIndex t = sorted_vocab_[root]; sorted_vocab_[root] = sorted_vocab_[swap]; sorted_vocab_[swap] = t;
            root = swap;
        }
    };
    for (int start = (vocab_size_ - 2) / 2; start >= 0; --start) sift(start, vocab_size_ - 1);
    for (int end = vocab_size_ - 1; end > 0; --end) {
        const TokenIndex t = sorted_vocab_[0]; sorted_vocab_[0] = sorted_vocab_[end]; sorted_vocab_[end] = t;
        sift(0, end - 1);
    }
    for (int i = 1; i < vocab_size_; i++) if (compare_str(sorted_vocab_[i - 1].str, sorted_vocab_[i].str) == 0) return LoadStatus::tokenizer_malformed;
    // run.c writes str_lookup(" ") unconditionally as the dummy prefix; require it to exist.
    if (str_lookup(" ") < 0) return LoadStatus::tokenizer_malformed;
    loaded_ = true;
    return LoadStatus::ok;
}

float* Engine::forward(int token, int pos) {
    const Config* p = &cfg_;
    const Weights* w = &w_;
    State* s = &s_;
    float* x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads;
    int hidden_dim = p->hidden_dim;
    int head_size = dim / p->n_heads;

    const float* content_row = w->token_embedding_table + token * dim;
    copy_bytes(x, content_row, size_t(dim) * sizeof(*x));

    for (unsigned long long l = 0; l < static_cast<unsigned long long>(p->n_layers); l++) {
        rmsnorm(s->xb, x, w->rms_att_weight + l * dim, dim);
        int loff = static_cast<int>(l * p->seq_len * kv_dim);
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;
        matmul(s->q, s->xb, w->wq + l * dim * dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l * dim * kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l * dim * kv_dim, dim, kv_dim);
        for (int i = 0; i < dim; i += 2) {
            int head_dim = i % head_size;
            float freq = 1.0f / pg_powf(10000.0f, head_dim / static_cast<float>(head_size));
            float val = pos * freq;
            float fcr = pg_cosf(val);
            float fci = pg_sinf(val);
            int rotn = i < kv_dim ? 2 : 1;
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k;
                float v0 = vec[i];
                float v1 = vec[i + 1];
                vec[i] = v0 * fcr - v1 * fci;
                vec[i + 1] = v0 * fci + v1 * fcr;
            }
        }
        for (int h = 0; h < p->n_heads; h++) {
            float* q = s->q + h * head_size;
            float* att = s->att + h * p->seq_len;
            for (int t = 0; t <= pos; t++) {
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) score += q[i] * k[i];
                score /= pg_sqrtf(static_cast<float>(head_size));
                att[t] = score;
            }
            softmax(att, pos + 1);
            float* xb = s->xb + h * head_size;
            zero_floats(xb, size_t(head_size));
            for (int t = 0; t <= pos; t++) {
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float a = att[t];
                for (int i = 0; i < head_size; i++) xb[i] += a * v[i];
            }
        }
        matmul(s->xb2, s->xb, w->wo + l * dim * dim, dim, dim);
        for (int i = 0; i < dim; i++) x[i] += s->xb2[i];
        rmsnorm(s->xb, x, w->rms_ffn_weight + l * dim, dim);
        matmul(s->hb, s->xb, w->w1 + l * dim * hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l * dim * hidden_dim, dim, hidden_dim);
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + pg_expf(-val)));
            val *= s->hb2[i];
            s->hb[i] = val;
        }
        matmul(s->xb, s->hb, w->w2 + l * dim * hidden_dim, hidden_dim, dim);
        for (int i = 0; i < dim; i++) x[i] += s->xb[i];
    }
    rmsnorm(x, x, w->rms_final_weight, dim);
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

int Engine::str_lookup(const char* str) const {
    int lo = 0, hi = vocab_size_ - 1;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        const int c = compare_str(str, sorted_vocab_[mid].str);
        if (c == 0) return sorted_vocab_[mid].id;
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

void Engine::encode(const char* text, int* tokens, int* n_tokens) {
    size_t str_len_ = 0;
    *n_tokens = 0;
    tokens[(*n_tokens)++] = 1;  // BOS
    if (text[0] != '\0') tokens[(*n_tokens)++] = str_lookup(" ");
    for (const char* c = text; *c != '\0'; c++) {
        if ((*c & 0xC0) != 0x80) str_len_ = 0;
        str_buffer_[str_len_++] = *c;
        str_buffer_[str_len_] = '\0';
        if ((*(c + 1) & 0xC0) == 0x80 && str_len_ < 4) continue;
        int id = str_lookup(str_buffer_);
        if (id != -1) tokens[(*n_tokens)++] = id;
        else for (size_t i = 0; i < str_len_; i++) tokens[(*n_tokens)++] = static_cast<unsigned char>(str_buffer_[i]) + 3;
        str_len_ = 0;
    }
    while (true) {
        float best_score = -1e10f;
        int best_id = -1;
        int best_idx = -1;
        for (int i = 0; i < (*n_tokens - 1); i++) {
            // sprintf("%s%s") -> bounded concatenation (both pieces <= max_token_length).
            const char* a = vocab_[tokens[i]]; const char* b = vocab_[tokens[i + 1]];
            const size_t la = str_len(a), lb = str_len(b);
            copy_bytes(str_buffer_, a, la); copy_bytes(str_buffer_ + la, b, lb); str_buffer_[la + lb] = '\0';
            int id = str_lookup(str_buffer_);
            if (id != -1 && vocab_scores_[id] > best_score) { best_score = vocab_scores_[id]; best_id = id; best_idx = i; }
        }
        if (best_idx == -1) break;
        tokens[best_idx] = best_id;
        for (int i = best_idx + 1; i < (*n_tokens - 1); i++) tokens[i] = tokens[i + 1];
        (*n_tokens)--;
    }
}

const char* Engine::decode(int prev_token, int token) const {
    const char* piece = vocab_[token];
    if (prev_token == 1 && piece[0] == ' ') piece++;
    // sscanf(piece, "<0x%02hhX>", &byte_val) == 1: "<0x" followed by one or two hex digits.
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x') {
        auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        const int h0 = hex(piece[3]);
        if (h0 >= 0) {
            const int h1 = hex(piece[4]);
            const int byte_val = h1 >= 0 ? h0 * 16 + h1 : h0;
            piece = reinterpret_cast<const char*>(byte_pieces_) + byte_val * 2;
        }
    }
    return piece;
}

int Engine::sample(float* logits, float temperature, float topp) {
    const int n = vocab_size_;
    if (temperature == 0.0f) {
        int max_i = 0; float max_p = logits[0];
        for (int i = 1; i < n; i++) if (logits[i] > max_p) { max_i = i; max_p = logits[i]; }
        return max_i;
    }
    for (int q = 0; q < n; q++) logits[q] /= temperature;
    softmax(logits, n);
    rng_state_ ^= rng_state_ >> 12; rng_state_ ^= rng_state_ << 25; rng_state_ ^= rng_state_ >> 27;
    const float coin = static_cast<unsigned int>((rng_state_ * 0x2545F4914F6CDD1Dull) >> 32 >> 8) / 16777216.0f;
    if (topp <= 0 || topp >= 1) {
        float cdf = 0.0f;
        for (int i = 0; i < n; i++) { cdf += logits[i]; if (coin < cdf) return i; }
        return n - 1;
    }
    // Top-p. run.c sorts candidates with qsort; insertion into a descending list gives the same
    // probabilities in the same order up to ties (equal probabilities may be ordered differently).
    int n0 = 0;
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) if (logits[i] >= cutoff) { probindex_[n0].index = i; probindex_[n0].prob = logits[i]; n0++; }
    for (int i = 1; i < n0; i++) {  // insertion sort, descending (n0 is small after the cutoff)
        const ProbIndex v = probindex_[i]; int j = i - 1;
        while (j >= 0 && probindex_[j].prob < v.prob) { probindex_[j + 1] = probindex_[j]; --j; }
        probindex_[j + 1] = v;
    }
    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1;
    for (int i = 0; i < n0; i++) { cumulative_prob += probindex_[i].prob; if (cumulative_prob > topp) { last_idx = i; break; } }
    const float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) { cdf += probindex_[i].prob; if (r < cdf) return probindex_[i].index; }
    return probindex_[last_idx].index;
}

int Engine::generate(const char* prompt, int steps, float temperature, float topp, uint64_t seed, Output out) {
    if (!loaded_ || !prompt || !out) return -1;
    if (str_len(prompt) > size_t(max_prompt_bytes)) return -1;
    if (steps <= 0 || steps > cfg_.seq_len) steps = cfg_.seq_len;
    rng_state_ = seed;
    int num_prompt_tokens = 0;
    encode(prompt, prompt_tokens_, &num_prompt_tokens);
    int printed = 0, next, token = prompt_tokens_[0], pos = 0;
    while (pos < steps) {
        float* logits = forward(token, pos);
        if (pos < num_prompt_tokens - 1) next = prompt_tokens_[pos + 1];
        else next = sample(logits, temperature, topp);
        pos++;
        if (next == 1) break;
        const char* piece = decode(token, next);
        // safe_printf: skip lone bytes that are neither printable nor whitespace.
        if (piece && piece[0] != '\0') {
            const unsigned char b = static_cast<unsigned char>(piece[0]);
            const bool lone = piece[1] == '\0';
            const bool printable = (b >= 0x20 && b <= 0x7E) || b == ' ' || (b >= '\t' && b <= '\r');
            if (!lone || printable) { out(piece); ++printed; }
        }
        token = next;
    }
    return printed;
}
}
