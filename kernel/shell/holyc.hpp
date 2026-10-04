#pragma once
#include <stddef.h>
#include <stdint.h>
namespace peregrinus::holyc {
// Bounded interpreter for a small HolyC subset (the language of TempleOS), used as the
// shell's scripting language. Hybrid model: the kernel stays C++; scripts run here with fixed
// limits and no access to memory, ports or kernel state. Every limit fails closed with an error.
//
// Supported: I64 variables (`I64 a = 1, b;`), = += -= *= /= %=, ++/-- statements,
// if/else, while, for, break, continue, blocks, // and /* */ comments,
// string statements that print (`"x=%d\n", x;`) and Print("...", args);
// expressions with || && == != < > <= >= + - * / % unary - ! and parentheses;
// formats %d %i %x %c %%. Arithmetic wraps (two's complement); division by zero is an error.
inline constexpr size_t max_source = 2048;
inline constexpr size_t max_vars = 32;
inline constexpr size_t max_ident = 15;
inline constexpr uint32_t max_steps = 200000;
inline constexpr uint32_t max_depth = 48;
inline constexpr size_t max_output = 8192;

using Output = void (*)(const char* utf8);
struct Result {
    bool ok;
    const char* error;  // static message (UTF-8), null when ok
    uint32_t line;      // 1-based source line of the error
};
// `source` is Latin-1 (as typed in the shell); output is UTF-8.
Result run(const char* source, size_t length, Output out);
}
