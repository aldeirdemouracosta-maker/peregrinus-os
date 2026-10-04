#include "service.hpp"
#include "engine.hpp"
#include <peregrinus/model_allowlist.h>
#include "../../third_party/limine/limine_min.h"
#include "../mm/memory.hpp"
#include "../security/sha256.hpp"
#include "../security/quarantine.hpp"
#include "../console/format.hpp"

namespace peregrinus::llm::service {
namespace {
Engine g_engine;
bool g_ready = false;
const char* g_name = "";
constexpr int steps_default = 128;
constexpr uint64_t component_base = 0x4C4C4D00;  // "LLM\0": Purgatorio IDs for allowlisted models

bool ends_with(const char* s, const char* suffix) {
    if (!s) return false;
    size_t n = 0, m = 0; while (s[n]) ++n; while (suffix[m]) ++m;
    if (m > n) return false;
    for (size_t i = 0; i < m; ++i) if (s[n - m + i] != suffix[i]) return false;
    return true;
}
const limine_file* find(const limine_module_response* r, const char* suffix) {
    if (!r) return nullptr;
    for (uint64_t i = 0; i < r->module_count; ++i) if (r->modules[i] && ends_with(r->modules[i]->path, suffix)) return r->modules[i];
    return nullptr;
}
uint64_t tsc() { uint32_t lo, hi; asm volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (uint64_t(hi) << 32) | lo; }
}

bool init(const limine_module_response* modules, Output log) {
    g_ready = false;
    const limine_file* m = find(modules, "/model.bin");
    const limine_file* t = find(modules, "/tokenizer.bin");
    if (!m || !t) { log("LLM: no model.bin/tokenizer.bin module; 'conversa' unavailable\n"); return false; }
    const auto* mb = static_cast<const uint8_t*>(m->address);
    const auto* tb = static_cast<const uint8_t*>(t->address);
    // Purgatorio admission: both digests must match one allowlisted entry.
    security::quarantine::Digest dm{}, dt{};
    security::sha256::digest(mb, m->size, dm.bytes);
    security::sha256::digest(tb, t->size, dt.bytes);
    auto& reg = security::quarantine::registry();
    const size_t n = sizeof(peregrinus_trusted_models) / sizeof(peregrinus_trusted_models[0]);
    for (size_t i = 0; i < n; ++i) {
        const auto& e = peregrinus_trusted_models[i];
        security::quarantine::Digest em{}, et{};
        for (int b = 0; b < 32; ++b) { em.bytes[b] = e.model_sha256[b]; et.bytes[b] = e.tokenizer_sha256[b]; }
        const uint64_t id = component_base + 2 * i;
        (void)reg.trust(id, security::quarantine::Kind::model, em);
        (void)reg.trust(id + 1, security::quarantine::Kind::model, et);
        if (reg.admit(id, dm) == security::quarantine::Admission::allow && reg.admit(id + 1, dt) == security::quarantine::Admission::allow) g_name = e.name;
    }
    if (!g_name[0]) { log("LLM: model SHA-256 not in the allowlist; refused (Purgatorio, fail-closed)\n"); return false; }
    const size_t need = arena_bytes_needed(mb, m->size, tb, t->size);
    if (!need) { log("LLM: malformed model/tokenizer; refused\n"); return false; }
    void* phys = memory::alloc_contiguous(need);
    auto* base = phys ? static_cast<uint8_t*>(memory::phys_to_hhdm(reinterpret_cast<uint64_t>(phys))) : nullptr;
    if (!base) { log("LLM: not enough memory for the model's working buffers\n"); return false; }
    Arena arena(base, need);
    const LoadStatus st = g_engine.load(mb, m->size, tb, t->size, arena);
    if (st != LoadStatus::ok) { log("LLM: load failed: "); log(load_status_name(st)); log("\n"); return false; }
    char d[24];
    log("LLM: model '"); log(g_name); log("' admitted (SHA-256 allowlist); dim ");
    format::dec64(uint64_t(g_engine.config().dim), d); log(d); log(", layers ");
    format::dec64(uint64_t(g_engine.config().n_layers), d); log(d); log(", vocab ");
    format::dec64(uint64_t(g_engine.config().vocab_size), d); log(d); log(", arena ");
    format::dec64(need / 1024, d); log(d); log(" KiB\n");
    g_ready = true;
    return true;
}
bool ready() { return g_ready; }
const char* model_name() { return g_name; }

void converse(const char* prompt, bool greedy, Output out) {
    if (!g_ready) { out("IA local indisponível: nenhum modelo autorizado foi carregado.\n"); return; }
    // Shell text is Latin-1; the model expects UTF-8.
    char utf8[2 * 160 + 1]; size_t n = 0;
    for (const char* p = prompt; *p && n + 2 < sizeof(utf8); ++p) {
        const uint8_t b = static_cast<uint8_t>(*p);
        if (b < 0x80) utf8[n++] = static_cast<char>(b);
        else { utf8[n++] = static_cast<char>(0xC0 | (b >> 6)); utf8[n++] = static_cast<char>(0x80 | (b & 0x3F)); }
    }
    utf8[n] = 0;
    const int r = g_engine.generate(utf8, steps_default, greedy ? 0.0f : 0.8f, 0.9f, greedy ? 1 : (tsc() | 1), out);
    out("\n");
    if (r < 0) out("Prompt longo demais.\n");
}
}
