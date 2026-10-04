// Runs the kernel LLM engine on the host and prints exactly what run.c prints on stdout
// (generated text followed by "\n"), so tests/llm.sh can diff both byte for byte.
// Usage: llm_test <model.bin> <tokenizer.bin> <temperature> <topp> <seed> <steps> <prompt>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "llm/engine.hpp"
static std::vector<uint8_t> slurp(const char* p) {
    std::vector<uint8_t> v; FILE* f = std::fopen(p, "rb"); if (!f) return v;
    int c; while ((c = std::fgetc(f)) != EOF) v.push_back(uint8_t(c)); std::fclose(f); return v;
}
int main(int argc, char** argv) {
    if (argc != 8) return 2;
    auto model = slurp(argv[1]), tok = slurp(argv[2]);
    const size_t need = peregrinus::llm::arena_bytes_needed(model.data(), model.size(), tok.data(), tok.size());
    if (!need) { std::puts("arena size: invalid files"); return 3; }
    std::vector<uint8_t> mem(need);
    peregrinus::llm::Arena arena(mem.data(), mem.size());
    peregrinus::llm::Engine e;
    const auto st = e.load(model.data(), model.size(), tok.data(), tok.size(), arena);
    if (st != peregrinus::llm::LoadStatus::ok) { std::printf("load: %s\n", peregrinus::llm::load_status_name(st)); return 4; }
    if (arena.used() > need) return 5;
    const int n = e.generate(argv[7], std::atoi(argv[6]), float(std::atof(argv[3])), float(std::atof(argv[4])), std::strtoull(argv[5], nullptr, 10),
                             [](const char* s) { std::fputs(s, stdout); });
    std::fputs("\n", stdout);
    return n < 0 ? 6 : 0;
}
