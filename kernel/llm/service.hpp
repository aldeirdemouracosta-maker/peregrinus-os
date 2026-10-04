#pragma once
#include <stdint.h>
struct limine_module_response;
namespace peregrinus::llm::service {
using Output = void (*)(const char* utf8);
// Finds model.bin + tokenizer.bin among the Limine modules, verifies both SHA-256 digests
// against the allowlist through the Purgatorio admission gate, reserves one contiguous arena
// and loads the engine. Any failure leaves the service unavailable. This file is compiled with
// SSE, so cpu::enable_sse() must run (from general-register code) before any call into it.
bool init(const limine_module_response* modules, Output log);
bool ready();
const char* model_name();
// Shell entry point: `prompt` is Latin-1 (as typed). greedy = deterministic (temperature 0).
void converse(const char* prompt, bool greedy, Output out);
}
