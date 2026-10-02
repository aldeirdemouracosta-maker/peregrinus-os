#pragma once
namespace peregrinus::serial {
bool init();
void putc(char c);
void write(const char* s);
void writeln(const char* s);
}
