#include "holyc.hpp"

namespace peregrinus::holyc {
namespace {

enum class Flow : uint8_t { normal, brk, cont, error };

struct Var { char name[max_ident + 1]; int64_t value; };

class Interp {
public:
    Interp(const char* s, size_t n, Output out) : src_(s), len_(n), out_(out) {}

    Result run() {
        if (len_ > max_source) return fail_now("programa grande demais (máximo 2048 bytes)");
        while (true) {
            skip_space();
            if (error_) break;
            if (pos_ >= len_) break;
            const Flow f = statement(true);
            if (f == Flow::error) break;
            if (f == Flow::brk || f == Flow::cont) { error("break/continue fora de um laço"); break; }
        }
        flush();
        if (error_) return {false, error_, line_of(error_pos_)};
        return {true, nullptr, 0};
    }

private:
    const char* src_; size_t len_; Output out_;
    size_t pos_ = 0;
    const char* error_ = nullptr; size_t error_pos_ = 0;
    uint32_t steps_ = 0, depth_ = 0;
    size_t out_total_ = 0;
    Var vars_[max_vars]{}; size_t nvars_ = 0;
    char obuf_[128]; size_t olen_ = 0;

    Result fail_now(const char* m) { return {false, m, 1}; }
    uint32_t line_of(size_t p) const { uint32_t l = 1; for (size_t i = 0; i < p && i < len_; ++i) if (src_[i] == '\n') ++l; return l; }
    Flow error(const char* m) { if (!error_) { error_ = m; error_pos_ = pos_; } return Flow::error; }
    bool step() { if (++steps_ > max_steps) { error("limite de passos excedido (200000): laço infinito?"); return false; } return true; }

    // ---- output (buffered; user text is Latin-1 and converted to UTF-8) ----
    void flush() { if (olen_) { obuf_[olen_] = 0; out_(obuf_); olen_ = 0; } }
    bool emit_byte(char c) {
        if (++out_total_ > max_output) { error("saída longa demais (máximo 8192 bytes)"); return false; }
        if (olen_ + 1 >= sizeof(obuf_)) flush();
        obuf_[olen_++] = c;
        return true;
    }
    bool emit_latin1(uint8_t b) {
        if (b < 0x80) return emit_byte(static_cast<char>(b));
        return emit_byte(static_cast<char>(0xC0 | (b >> 6))) && emit_byte(static_cast<char>(0x80 | (b & 0x3F)));
    }

    // ---- lexing ----
    char peek(size_t o = 0) const { return pos_ + o < len_ ? src_[pos_ + o] : 0; }
    static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static bool is_digit(char c) { return c >= '0' && c <= '9'; }
    void skip_space() {
        while (pos_ < len_) {
            const char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos_; continue; }
            if (c == '/' && peek(1) == '/') { while (pos_ < len_ && src_[pos_] != '\n') ++pos_; continue; }
            if (c == '/' && peek(1) == '*') {
                pos_ += 2;
                while (pos_ + 1 < len_ && !(src_[pos_] == '*' && src_[pos_ + 1] == '/')) ++pos_;
                if (pos_ + 1 >= len_) { error("comentário /* sem */"); pos_ = len_; return; }
                pos_ += 2; continue;
            }
            break;
        }
    }
    bool accept(char c) { skip_space(); if (peek() == c) { ++pos_; return true; } return false; }
    bool accept2(char a, char b) { skip_space(); if (peek() == a && peek(1) == b) { pos_ += 2; return true; } return false; }
    bool expect(char c, const char* m) { if (accept(c)) return true; error(m); return false; }
    // Reads an identifier into `id`; returns false if none (position unchanged).
    bool ident(char (&id)[max_ident + 1]) {
        skip_space();
        if (!is_alpha(peek())) return false;
        size_t n = 0; const size_t start = pos_;
        while (is_alpha(peek()) || is_digit(peek())) {
            if (n >= max_ident) { pos_ = start; error("nome longo demais (máximo 15 caracteres)"); return false; }
            id[n++] = src_[pos_++];
        }
        id[n] = 0;
        return true;
    }
    bool keyword(const char* k) {
        skip_space();
        size_t n = 0; while (k[n]) { if (peek(n) != k[n]) return false; ++n; }
        if (is_alpha(peek(n)) || is_digit(peek(n))) return false;
        pos_ += n; return true;
    }
    static bool eq(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }

    // ---- variables ----
    Var* find(const char* n) { for (size_t i = 0; i < nvars_; ++i) if (eq(vars_[i].name, n)) return &vars_[i]; return nullptr; }
    Var* declare(const char* n) {
        if (Var* v = find(n)) return v;  // redeclaration (e.g. inside a loop body) re-initializes
        if (nvars_ >= max_vars) { error("variáveis demais (máximo 32)"); return nullptr; }
        Var& v = vars_[nvars_++];
        size_t i = 0; for (; n[i]; ++i) v.name[i] = n[i]; v.name[i] = 0; v.value = 0;
        return &v;
    }
    static bool reserved(const char* n) {
        static const char* const words[] = {"I64", "if", "else", "while", "for", "break", "continue", "Print"};
        for (const char* w : words) if (eq(n, w)) return true;
        return false;
    }

    // ---- expressions (wrapping arithmetic through uint64_t: no signed-overflow UB) ----
    static int64_t wrap(uint64_t v) { return static_cast<int64_t>(v); }
    bool enter() { if (++depth_ > max_depth) { error("aninhamento profundo demais"); return false; } return true; }
    void leave() { --depth_; }

    int64_t expr(bool ex) { return logic_or(ex); }
    int64_t logic_or(bool ex) {
        int64_t v = logic_and(ex);
        while (!error_ && accept2('|', '|')) { const bool rhs_ex = ex && !v; const int64_t r = logic_and(rhs_ex); v = (v || r) ? 1 : 0; }
        return v;
    }
    int64_t logic_and(bool ex) {
        int64_t v = equality(ex);
        while (!error_ && accept2('&', '&')) { const bool rhs_ex = ex && v; const int64_t r = equality(rhs_ex); v = (v && r) ? 1 : 0; }
        return v;
    }
    int64_t equality(bool ex) {
        int64_t v = relational(ex);
        while (!error_) {
            if (accept2('=', '=')) v = v == relational(ex);
            else if (accept2('!', '=')) v = v != relational(ex);
            else break;
        }
        return v;
    }
    int64_t relational(bool ex) {
        int64_t v = additive(ex);
        while (!error_) {
            if (accept2('<', '=')) v = v <= additive(ex);
            else if (accept2('>', '=')) v = v >= additive(ex);
            else if (peek_op('<')) { ++pos_; v = v < additive(ex); }
            else if (peek_op('>')) { ++pos_; v = v > additive(ex); }
            else break;
        }
        return v;
    }
    // A single-char operator that is not the start of a two-char one (<=, >=, <<, >>).
    bool peek_op(char c) { skip_space(); return peek() == c && peek(1) != '=' && peek(1) != c; }
    int64_t additive(bool ex) {
        int64_t v = term(ex);
        while (!error_) {
            skip_space();
            if (peek() == '+' && peek(1) != '+' && peek(1) != '=') { ++pos_; v = wrap(uint64_t(v) + uint64_t(term(ex))); }
            else if (peek() == '-' && peek(1) != '-' && peek(1) != '=') { ++pos_; v = wrap(uint64_t(v) - uint64_t(term(ex))); }
            else break;
        }
        return v;
    }
    int64_t divide(int64_t a, int64_t b, bool mod, bool ex) {
        if (!ex) return 0;
        if (b == 0) { error("divisão por zero"); return 0; }
        if (a == INT64_MIN && b == -1) return mod ? 0 : a;  // wraps, like the hardware would trap
        return mod ? a % b : a / b;
    }
    int64_t term(bool ex) {
        int64_t v = unary(ex);
        while (!error_) {
            skip_space();
            if (peek() == '*' && peek(1) != '=') { ++pos_; v = wrap(uint64_t(v) * uint64_t(unary(ex))); }
            else if (peek() == '/' && peek(1) != '=' && peek(1) != '/' && peek(1) != '*') { ++pos_; const int64_t r = unary(ex); v = divide(v, r, false, ex); }
            else if (peek() == '%' && peek(1) != '=') { ++pos_; const int64_t r = unary(ex); v = divide(v, r, true, ex); }
            else break;
        }
        return v;
    }
    int64_t unary(bool ex) {
        if (!enter()) return 0;
        int64_t v;
        skip_space();
        if (peek() == '-' && peek(1) != '-') { ++pos_; v = wrap(0 - uint64_t(unary(ex))); }
        else if (peek() == '!' && peek(1) != '=') { ++pos_; v = !unary(ex); }
        else v = primary(ex);
        leave();
        return v;
    }
    int64_t primary(bool ex) {
        if (ex && !step()) return 0;
        skip_space();
        if (accept('(')) { const int64_t v = expr(ex); expect(')', "esperado ')'"); return v; }
        if (is_digit(peek())) return number();
        if (peek() == '\'') {  // character literal 'a'
            ++pos_; const uint8_t c = static_cast<uint8_t>(peek());
            if (!c || c == '\'' || c == '\n') { error("caractere inválido"); return 0; }
            ++pos_; if (!expect('\'', "esperado ' no fim do caractere")) return 0;
            return c;
        }
        char id[max_ident + 1];
        if (ident(id)) {
            if (reserved(id)) { error("palavra reservada usada como valor"); return 0; }
            Var* v = find(id);
            if (!v) { if (ex) error("variável não declarada"); return 0; }
            return v->value;
        }
        error("expressão esperada");
        return 0;
    }
    int64_t number() {
        uint64_t v = 0;
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            pos_ += 2; int digits = 0;
            while (true) {
                const char c = peek(); int d;
                if (is_digit(c)) d = c - '0'; else if (c >= 'a' && c <= 'f') d = c - 'a' + 10; else if (c >= 'A' && c <= 'F') d = c - 'A' + 10; else break;
                if (++digits > 16) { error("número grande demais"); return 0; }
                v = v * 16 + uint64_t(d); ++pos_;
            }
            if (!digits) { error("número hexadecimal vazio"); return 0; }
            return wrap(v);
        }
        while (is_digit(peek())) {
            const uint64_t d = uint64_t(peek() - '0');
            if (v > (uint64_t(INT64_MAX) - d) / 10) { error("número grande demais"); return 0; }
            v = v * 10 + d; ++pos_;
        }
        if (is_alpha(peek())) { error("número inválido"); return 0; }
        return int64_t(v);
    }

    // ---- statements ----
    // `simple`: assignment, compound assignment, ++/--, or (allow_decl) an I64 declaration.
    Flow simple(bool ex, bool allow_decl) {
        if (allow_decl && keyword("I64")) return declaration(ex);
        char id[max_ident + 1];
        if (!ident(id)) return error("comando esperado");
        if (reserved(id)) return error("palavra reservada fora de lugar");
        Var* v = find(id);
        if (!v && ex) return error("variável não declarada");
        if (ex && !step()) return Flow::error;
        skip_space();
        const char a = peek(), b = peek(1);
        if (a == '+' && b == '+') { pos_ += 2; if (ex) v->value = wrap(uint64_t(v->value) + 1); return Flow::normal; }
        if (a == '-' && b == '-') { pos_ += 2; if (ex) v->value = wrap(uint64_t(v->value) - 1); return Flow::normal; }
        char op = 0;
        if (b == '=' && (a == '+' || a == '-' || a == '*' || a == '/' || a == '%')) { op = a; pos_ += 2; }
        else if (a == '=' && b != '=') { op = '='; ++pos_; }
        else return error("esperado '=', '+=', '-=', '*=', '/=', '%=', '++' ou '--'");
        const int64_t r = expr(ex);
        if (error_ || !ex) return error_ ? Flow::error : Flow::normal;
        switch (op) {
            case '=': v->value = r; break;
            case '+': v->value = wrap(uint64_t(v->value) + uint64_t(r)); break;
            case '-': v->value = wrap(uint64_t(v->value) - uint64_t(r)); break;
            case '*': v->value = wrap(uint64_t(v->value) * uint64_t(r)); break;
            case '/': v->value = divide(v->value, r, false, true); break;
            case '%': v->value = divide(v->value, r, true, true); break;
        }
        return error_ ? Flow::error : Flow::normal;
    }
    Flow declaration(bool ex) {
        do {
            char id[max_ident + 1];
            if (!ident(id)) return error("nome de variável esperado depois de I64");
            if (reserved(id)) return error("palavra reservada não pode ser variável");
            Var* v = ex ? declare(id) : nullptr;
            if (ex && !v) return Flow::error;
            if (accept('=')) { const int64_t r = expr(ex); if (error_) return Flow::error; if (ex) v->value = r; }
            else if (ex) v->value = 0;
        } while (accept(','));
        return Flow::normal;
    }
    // String literal: prints with printf-style formatting, consuming ", expr" arguments.
    Flow print_string(bool ex, bool in_print_call) {
        if (!accept('"')) return error("esperado \"");
        const size_t start = pos_;
        while (pos_ < len_ && src_[pos_] != '"') { if (src_[pos_] == '\\' && pos_ + 1 < len_) ++pos_; if (src_[pos_] == '\n') return error("string sem \" final"); ++pos_; }
        if (pos_ >= len_) return error("string sem \" final");
        const size_t end = pos_++;
        // Evaluate up to 8 arguments now (left to right), then format.
        int64_t args[8]; size_t nargs = 0;
        while (accept(',')) {
            if (nargs >= 8) return error("argumentos demais (máximo 8)");
            args[nargs++] = expr(ex);
            if (error_) return Flow::error;
        }
        if (in_print_call && !expect(')', "esperado ')' no fim de Print")) return Flow::error;
        if (!ex) return Flow::normal;
        if (!step()) return Flow::error;
        size_t used = 0;
        for (size_t i = start; i < end; ++i) {
            char c = src_[i];
            if (c == '\\' && i + 1 < end) {
                const char e = src_[++i];
                c = e == 'n' ? '\n' : e == 't' ? '\t' : e == '\\' ? '\\' : e == '"' ? '"' : e == '\'' ? '\'' : e == '0' ? 0 : 0;
                if (c == 0 && e != '0') return error("escape inválido em string");
                if (c == 0) continue;
                if (!emit_byte(c)) return Flow::error;
                continue;
            }
            if (c == '%' && i + 1 < end) {
                const char f = src_[++i];
                if (f == '%') { if (!emit_byte('%')) return Flow::error; continue; }
                if (used >= nargs) return error("faltam argumentos para o formato");
                const int64_t a = args[used++];
                if (f == 'd' || f == 'i') { if (!emit_dec(a)) return Flow::error; }
                else if (f == 'x' || f == 'X') { if (!emit_hex(uint64_t(a), f == 'X')) return Flow::error; }
                else if (f == 'c') { if (a < 0 || a > 255) return error("%c fora de 0..255"); if (!emit_latin1(uint8_t(a))) return Flow::error; }
                else return error("formato não suportado (use %d %x %c %%)");
                continue;
            }
            if (!emit_latin1(static_cast<uint8_t>(c))) return Flow::error;
        }
        if (used != nargs) return error("argumentos sobrando para o formato");
        return Flow::normal;
    }
    bool emit_dec(int64_t v) {
        char t[21]; int i = 20; t[i] = 0;
        uint64_t u = v < 0 ? uint64_t(0) - uint64_t(v) : uint64_t(v);
        do { t[--i] = char('0' + u % 10); u /= 10; } while (u);
        if (v < 0 && !emit_byte('-')) return false;
        for (; t[i]; ++i) if (!emit_byte(t[i])) return false;
        return true;
    }
    bool emit_hex(uint64_t v, bool upper) {
        char t[17]; int i = 16; t[i] = 0;
        do { const int d = int(v & 15); t[--i] = char(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10); v >>= 4; } while (v);
        for (; t[i]; ++i) if (!emit_byte(t[i])) return false;
        return true;
    }

    Flow statement(bool ex) {
        if (!enter()) return Flow::error;
        const Flow f = statement_body(ex);
        leave();
        return f;
    }
    Flow statement_body(bool ex) {
        skip_space();
        if (error_) return Flow::error;
        if (pos_ >= len_) return error("fim inesperado do programa");
        if (ex && !step()) return Flow::error;
        if (accept(';')) return Flow::normal;
        if (accept('{')) {
            Flow result = Flow::normal;
            while (true) {
                skip_space();
                if (error_) return Flow::error;
                if (accept('}')) return result;
                if (pos_ >= len_) return error("bloco sem '}'");
                const Flow f = statement(ex && result == Flow::normal);
                if (f == Flow::error) return f;
                if (f != Flow::normal && result == Flow::normal) result = f;  // keep parsing the rest, skipping it
            }
        }
        skip_space();
        if (peek() == '"') { const Flow f = print_string(ex, false); if (f == Flow::error) return f; return expect(';', "esperado ';'") ? Flow::normal : Flow::error; }
        if (keyword("Print")) {
            if (!expect('(', "esperado '(' depois de Print")) return Flow::error;
            const Flow f = print_string(ex, true); if (f == Flow::error) return f;
            return expect(';', "esperado ';'") ? Flow::normal : Flow::error;
        }
        if (keyword("if")) {
            if (!expect('(', "esperado '(' depois de if")) return Flow::error;
            const int64_t c = expr(ex); if (error_) return Flow::error;
            if (!expect(')', "esperado ')'")) return Flow::error;
            Flow f = statement(ex && c);
            if (f == Flow::error) return f;
            if (keyword("else")) {
                const Flow g = statement(ex && !c);
                if (g == Flow::error) return g;
                if (ex && !c) f = g;
            }
            return ex ? f : Flow::normal;
        }
        if (keyword("while")) {
            if (!expect('(', "esperado '(' depois de while")) return Flow::error;
            const size_t cond = pos_;
            while (true) {
                pos_ = cond;
                const int64_t c = expr(ex); if (error_) return Flow::error;
                if (!expect(')', "esperado ')'")) return Flow::error;
                const size_t body = pos_;
                const bool run_body = ex && c;
                const Flow f = statement(run_body);
                if (f == Flow::error) return f;
                if (!run_body) return Flow::normal;
                if (f == Flow::brk) { pos_ = body; return statement(false) == Flow::error ? Flow::error : Flow::normal; }
            }
        }
        if (keyword("for")) {
            if (!expect('(', "esperado '(' depois de for")) return Flow::error;
            if (!accept(';')) { if (simple(ex, true) == Flow::error) return Flow::error; if (!expect(';', "esperado ';' no for")) return Flow::error; }
            const size_t cond = pos_;
            while (true) {
                pos_ = cond;
                int64_t c = 1;
                if (!accept(';')) { c = expr(ex); if (error_) return Flow::error; if (!expect(';', "esperado ';' no for")) return Flow::error; }
                const size_t step_pos = pos_;
                if (!accept(')')) { if (simple(false, false) == Flow::error) return Flow::error; if (!expect(')', "esperado ')' no for")) return Flow::error; }
                const size_t body = pos_;
                const bool run_body = ex && c;
                const Flow f = statement(run_body);
                if (f == Flow::error) return f;
                if (!run_body) return Flow::normal;
                if (f == Flow::brk) { pos_ = body; return statement(false) == Flow::error ? Flow::error : Flow::normal; }
                const size_t after = pos_;
                pos_ = step_pos;
                if (!accept(')')) { if (simple(true, false) == Flow::error) return Flow::error; }
                pos_ = after;
            }
        }
        if (keyword("break")) { if (!expect(';', "esperado ';'")) return Flow::error; return ex ? Flow::brk : Flow::normal; }
        if (keyword("continue")) { if (!expect(';', "esperado ';'")) return Flow::error; return ex ? Flow::cont : Flow::normal; }
        if (keyword("else")) return error("else sem if");
        const Flow f = simple(ex, true);
        if (f == Flow::error) return f;
        return expect(';', "esperado ';'") ? Flow::normal : Flow::error;
    }
};
}

Result run(const char* source, size_t length, Output out) {
    if (!source || !out) return {false, "entrada inválida", 1};
    Interp i(source, length, out);
    return i.run();
}
}
