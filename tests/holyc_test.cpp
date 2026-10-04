// HolyC subset interpreter: semantics, every limit and every error path.
#include <cstdio>
#include <cstring>
#include <string>
#include "shell/holyc.hpp"
using namespace peregrinus;
static std::string g;
static void sink(const char* s) { g += s; }
static holyc::Result run(const std::string& src) { g.clear(); return holyc::run(src.data(), src.size(), sink); }
static int fails = 0;
static void ok(const char* name, const std::string& src, const std::string& want) {
    const auto r = run(src);
    if (!r.ok || g != want) { std::printf("FAIL %s: ok=%d err=%s out=[%s] want=[%s]\n", name, r.ok, r.error ? r.error : "-", g.c_str(), want.c_str()); ++fails; }
}
static void bad(const char* name, const std::string& src, const char* err_part, uint32_t line = 0) {
    const auto r = run(src);
    if (r.ok || !r.error || !std::strstr(r.error, err_part) || (line && r.line != line)) {
        std::printf("FAIL %s: ok=%d err=%s line=%u (want '%s' line %u)\n", name, r.ok, r.error ? r.error : "-", r.line, err_part, line); ++fails;
    }
}
int main() {
    ok("hello", "\"Ol\xE1, Peregrinus!\\n\";", "Olá, Peregrinus!\n");                 // Latin-1 source -> UTF-8 output
    ok("format", "I64 x = 42; \"x=%d hex=%x %c%%\\n\", x, 255, 'A';", "x=42 hex=ff A%\n");
    ok("print call", "Print(\"%d-%d\\n\", 1+2*3, (1+2)*3);", "7-9\n");
    ok("precedence", "\"%d %d %d %d\", 7/2, -7/2, 7%3, -7%3;", "3 -3 1 -1");
    ok("logic", "\"%d%d%d%d%d\", 1&&0, 1||0, !5, 2<3 && 3<=3, 4!=4 || 5>=6;", "01010");
    ok("compound", "I64 a=10; a+=5; a-=3; a*=2; a/=5; a%=3; a++; a++; a--; \"%d\", a;", "2");
    ok("if else", "I64 n=5; if (n>3) \"big\"; else \"small\";", "big");
    ok("while", "I64 i=0,s=0; while (i<10) { s+=i; i++; } \"%d\", s;", "45");
    ok("for basic", "I64 s=0; for (I64 i=1; i<=5; i++) s+=i; \"%d\", s;", "15");
    ok("break continue", "I64 i; for (i=0; i<100; i++) { if (i%2) continue; if (i>8) break; \"%d \", i; }", "0 2 4 6 8 ");
    ok("nested loops", "I64 i,j,c=0; for(i=0;i<5;i++) for(j=0;j<5;j++) { if (j==i) break; c++; } \"%d\", c;", "10");
    ok("fizzbuzz", "I64 i; for (i=1;i<=15;i++) { if (i%15==0) \"FizzBuzz \"; else if (i%3==0) \"Fizz \"; else if (i%5==0) \"Buzz \"; else \"%d \", i; }",
       "1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz ");
    ok("comments hex", "/* bloco */ I64 x = 0x1F; // linha\n \"%d\", x;", "31");
    ok("wrap", "I64 m = 0x7FFFFFFFFFFFFFFF; m++; \"%d\", m;", "-9223372036854775808");
    ok("min div", "I64 m = 0x8000000000000000; \"%d %d\", m / -1, m % -1;", "-9223372036854775808 0");
    ok("short circuit", "I64 z=0; if (z && 1/z) \"no\"; else \"safe\";", "safe");
    ok("redeclare in loop", "I64 k; for (k=0;k<3;k++) { I64 t = k*2; \"%d\", t; }", "024");
    ok("empty", "", "");
    // errors (fail-closed, with line numbers)
    bad("comma op", "I64 s=0; for (I64 i=1; i<=5; i++) s*=1, s+=i;", "esperado ';'");
    bad("div zero", "I64 a=1;\nI64 b=0;\n\"%d\", a/b;", "divisão por zero", 3);
    bad("undeclared", "x = 3;", "não declarada");
    bad("infinite", "while (1) ;", "limite de passos");
    bad("infinite block", "for (;;) {}", "limite de passos");
    bad("too many vars", [] { std::string s; for (int i = 0; i < 33; ++i) s += "I64 v" + std::to_string(i) + ";"; return s; }(), "variáveis demais");
    bad("long name", "I64 abcdefghijklmnop;", "nome longo demais");
    bad("deep nesting", "I64 x = " + std::string(60, '(') + "1" + std::string(60, ')') + ";", "aninhamento");
    bad("deep blocks", std::string(60, '{') + std::string(60, '}'), "aninhamento");
    bad("output flood", "I64 i; for(i=0;i<5000;i++) \"0123456789\";", "saída longa demais");
    bad("big number", "I64 x = 99999999999999999999;", "grande demais");
    bad("bad format", "\"%s\", 1;", "formato não suportado");
    bad("missing arg", "\"%d %d\", 1;", "faltam argumentos");
    bad("extra arg", "\"%d\", 1, 2;", "sobrando");
    bad("unterminated string", "\"abc", "sem \" final");
    bad("unterminated comment", "/* abc", "comentário");
    bad("break outside", "break;", "fora de um laço");
    bad("else alone", "else \"x\";", "else sem if");
    bad("reserved", "I64 while = 1;", "reservada");
    bad("too big", std::string(holyc::max_source + 1, ' '), "grande demais");
    bad("garbage", "@#$", "comando esperado");
    // A failing program prints what it printed before the error (bounded, flushed).
    run("\"antes\"; I64 z = 1/0;");
    if (g != "antes") { std::printf("FAIL partial output: [%s]\n", g.c_str()); ++fails; }
    return fails ? 1 : 0;
}
