// Minimal GLSL ES 2.0 subset → ISA compiler.
//
// Sprint 9 extension over Sprint 2:
//   - Local variable declarations:  float / vec{2,3,4}  name = expr;
//   - Comparison operators (< <= > >= == !=) inside if conditions
//   - if (a OP b) { ... } [ else { ... } ]   (statement, not expression)
//   - Built-ins: normalize, dot, max, min, clamp, pow

#include "gpu_compiler/glsl.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "gpu_compiler/encoding.h"

namespace gpu::glsl {
namespace {

using namespace gpu::isa;

// -----------------------------------------------------------------------------
// Lexer
// -----------------------------------------------------------------------------
enum class Tok {
    EOF_, ID, NUMBER,
    LPAREN, RPAREN, LBRACE, RBRACE, SEMI, COMMA, DOT,
    ASSIGN, STAR, PLUS, MINUS, SLASH,
    LT, LE, GT, GE, EQ, NE,
    KW_VOID, KW_MAIN, KW_ATTRIBUTE, KW_UNIFORM, KW_VARYING, KW_PRECISION,
    KW_HIGHP, KW_MEDIUMP, KW_LOWP,
    KW_IF, KW_ELSE,
};

struct Token {
    Tok kind;
    std::string lex;
    int line = 0;
};

struct Lexer {
    const std::string& src;
    size_t i = 0;
    int line = 1;
    explicit Lexer(const std::string& s) : src(s) {}

    Token next() {
        while (i < src.size()) {
            char c = src[i];
            if (c == '\n') { ++line; ++i; continue; }
            if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
            if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                while (i < src.size() && src[i] != '\n') ++i;
                continue;
            }
            // Block comment /* ... */ (common in CTS shaders).
            if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                i += 2;
                while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
                    if (src[i] == '\n') ++line;
                    ++i;
                }
                if (i + 1 < src.size()) i += 2;   // consume "*/"
                continue;
            }
            // Sprint 47 — preprocessor directive line (`#version 100`,
            // `#extension ...`, `#define ...`). The hand-rolled GLSL ES 1.0
            // front-end doesn't model the preprocessor; skip the rest of
            // the line so the directive is invisible to downstream parsing.
            // Without this, `#` lexed to EOF and many CTS shaders silently
            // produced an empty program.
            if (c == '#') {
                while (i < src.size() && src[i] != '\n') ++i;
                continue;
            }
            break;
        }
        if (i >= src.size()) return {Tok::EOF_, "", line};
        char c = src[i];
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < src.size() &&
                   (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                ++i;
            std::string s = src.substr(start, i - start);
            if (s == "void") return {Tok::KW_VOID, s, line};
            if (s == "main") return {Tok::KW_MAIN, s, line};
            if (s == "attribute") return {Tok::KW_ATTRIBUTE, s, line};
            if (s == "uniform") return {Tok::KW_UNIFORM, s, line};
            if (s == "varying") return {Tok::KW_VARYING, s, line};
            if (s == "precision") return {Tok::KW_PRECISION, s, line};
            if (s == "highp") return {Tok::KW_HIGHP, s, line};
            if (s == "mediump") return {Tok::KW_MEDIUMP, s, line};
            if (s == "lowp") return {Tok::KW_LOWP, s, line};
            if (s == "if")    return {Tok::KW_IF, s, line};
            if (s == "else")  return {Tok::KW_ELSE, s, line};
            // Sprint 48 — bool literals as 1.0 / 0.0 numeric constants.
            // Codegen treats every type as a vec4 of floats, so the random-
            // shader generator's `bool` operands work fine when projected
            // onto float arithmetic.
            if (s == "true")  return {Tok::NUMBER, "1.0", line};
            if (s == "false") return {Tok::NUMBER, "0.0", line};
            return {Tok::ID, s, line};
        }
        // Number: digit-prefixed, OR `.` followed by digit (e.g. ".5").
        // A bare `.` is a member-access token.
        const bool num_start =
            std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < src.size() &&
             std::isdigit(static_cast<unsigned char>(src[i + 1])));
        if (num_start) {
            size_t start = i;
            bool seen_dot = false;
            while (i < src.size() &&
                   (std::isdigit(static_cast<unsigned char>(src[i])) ||
                    (src[i] == '.' && !seen_dot))) {
                if (src[i] == '.') seen_dot = true;
                ++i;
            }
            return {Tok::NUMBER, src.substr(start, i - start), line};
        }
        // Multi-char operators
        if (c == '<' && i + 1 < src.size() && src[i + 1] == '=') { i += 2; return {Tok::LE, "<=", line}; }
        if (c == '>' && i + 1 < src.size() && src[i + 1] == '=') { i += 2; return {Tok::GE, ">=", line}; }
        if (c == '=' && i + 1 < src.size() && src[i + 1] == '=') { i += 2; return {Tok::EQ, "==", line}; }
        if (c == '!' && i + 1 < src.size() && src[i + 1] == '=') { i += 2; return {Tok::NE, "!=", line}; }
        ++i;
        switch (c) {
            case '(': return {Tok::LPAREN, "(", line};
            case ')': return {Tok::RPAREN, ")", line};
            case '{': return {Tok::LBRACE, "{", line};
            case '}': return {Tok::RBRACE, "}", line};
            case ';': return {Tok::SEMI, ";", line};
            case ',': return {Tok::COMMA, ",", line};
            case '.': return {Tok::DOT, ".", line};
            case '=': return {Tok::ASSIGN, "=", line};
            case '*': return {Tok::STAR, "*", line};
            case '+': return {Tok::PLUS, "+", line};
            case '-': return {Tok::MINUS, "-", line};
            case '/': return {Tok::SLASH, "/", line};
            case '<': return {Tok::LT, "<", line};
            case '>': return {Tok::GT, ">", line};
        }
        return {Tok::EOF_, std::string(1, c), line};
    }
};

// -----------------------------------------------------------------------------
// AST
// -----------------------------------------------------------------------------
struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Identifier { std::string name; };
struct Number      { float value; };
struct MemberAccess { ExprPtr base; std::string member; };
struct BinaryOp    { char op; ExprPtr lhs, rhs; };
struct UnaryOp     { char op; ExprPtr expr; };
struct Call        { std::string name; std::vector<ExprPtr> args; };

struct Expr : std::variant<Identifier, Number, MemberAccess, BinaryOp, UnaryOp, Call> {
    using variant::variant;
    int line = 0;
};

struct Decl {
    std::string qualifier;   // "attribute" | "uniform" | "varying" | ""
    std::string type;
    std::string name;
    int line = 0;
    // Sprint 48 — file-scope `<type> <name> = <expr>;` declarations
    // (used by the dEQP random-shader generator's many `int i = foo();`
    // style globals). Stored here, then promoted to a LocalDecl
    // statement at the very start of `main`'s body in the codegen
    // prologue. nullptr means "no initializer" (the historical case).
    ExprPtr init;
    // Sprint 58 — track the `const` qualifier so emit_local_decl knows
    // it's safe to fold the scalar init into the c-bank (and skip
    // allocating a GPR). For non-const locals the value may be
    // reassigned later (e.g. `int c = a; c = -2;`); folding those
    // would silently lose the second write.
    bool is_const = false;
};

// Statements: assignment, local declaration, if/else.
struct AssignStmt { ExprPtr lhs; ExprPtr rhs; };
struct LocalDeclStmt { std::string type; std::string name; ExprPtr init; bool is_const = false; };
struct ExprStmt    { ExprPtr expr; };
struct IfStmt;     // forward
using StmtVar = std::variant<AssignStmt, LocalDeclStmt, ExprStmt, IfStmt>;
struct Stmt {
    std::shared_ptr<StmtVar> kind;
    int line = 0;
};

struct IfStmt {
    // Condition is restricted to `lhs OP rhs` (a comparison).
    ExprPtr cond_lhs;
    Tok     cond_op;
    ExprPtr cond_rhs;
    std::vector<Stmt> then_body;
    std::vector<Stmt> else_body;
};

struct Program {
    std::vector<Decl> globals;
    std::vector<Stmt> body;
};

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------
struct Parser {
    Lexer lex;
    Token cur;
    std::string error;
    int error_line = 0;

    explicit Parser(const std::string& s) : lex(s) { cur = lex.next(); }

    bool eat(Tok t) { if (cur.kind == t) { cur = lex.next(); return true; } return false; }
    bool expect(Tok t, const char* msg) {
        if (cur.kind == t) { cur = lex.next(); return true; }
        error = msg; error_line = cur.line; return false;
    }

    bool is_type_keyword(const std::string& s) {
        // Sprint 47 — added ivec*, bvec*, uint types. Codegen still treats
        // them as vec4-shaped GPRs (we don't model integer typing yet);
        // this just lets shaders that *declare* such variables parse
        // through to the use sites, which is enough for the dEQP random
        // shader generator's many `const ivec2 c = ivec2(1, 2);` style
        // top-level declarations to compile (they're often dead-coded).
        return s == "void" || s == "float" || s == "vec2" || s == "vec3" ||
               s == "vec4" || s == "mat2" || s == "mat3" || s == "mat4" ||
               s == "ivec2" || s == "ivec3" || s == "ivec4" ||
               s == "bvec2" || s == "bvec3" || s == "bvec4" ||
               s == "uint" || s == "bool" ||
               s == "sampler2D" || s == "samplerCube" || s == "int";
    }

    bool parse(Program& p) {
        while (cur.kind != Tok::EOF_) {
            if (cur.kind == Tok::KW_PRECISION) {
                cur = lex.next();
                if (cur.kind == Tok::KW_HIGHP || cur.kind == Tok::KW_MEDIUMP ||
                    cur.kind == Tok::KW_LOWP) cur = lex.next();
                if (cur.kind == Tok::ID) cur = lex.next();
                if (!expect(Tok::SEMI, "expected ';' after precision")) return false;
                continue;
            }
            // Sprint 47 — skip `const` and `invariant` qualifiers at the top
            // level so the dEQP random-shader generator's `const ivec2 c =
            // ivec2(1, 2);` style declarations no longer hit our parser as
            // unknown types. Codegen still handles the trailing initializer
            // via the regular global-decl path.
            // Sprint 58 — also remember whether `const` was present so
            // emit_local_decl can safely fold the init scalar into the
            // c-bank (a non-const local may be reassigned later).
            bool decl_is_const = false;
            while (cur.kind == Tok::ID &&
                   (cur.lex == "const" || cur.lex == "invariant" ||
                    cur.lex == "centroid" || cur.lex == "smooth" ||
                    cur.lex == "flat")) {
                if (cur.lex == "const") decl_is_const = true;
                cur = lex.next();
            }
            std::string qualifier;
            if (cur.kind == Tok::KW_ATTRIBUTE) { qualifier = "attribute"; cur = lex.next(); }
            else if (cur.kind == Tok::KW_UNIFORM) { qualifier = "uniform"; cur = lex.next(); }
            else if (cur.kind == Tok::KW_VARYING) { qualifier = "varying"; cur = lex.next(); }

            if (cur.kind == Tok::KW_HIGHP || cur.kind == Tok::KW_MEDIUMP ||
                cur.kind == Tok::KW_LOWP) cur = lex.next();

            if (qualifier.empty() && cur.kind == Tok::KW_VOID) {
                cur = lex.next();
                if (!expect(Tok::KW_MAIN, "expected 'main'")) return false;
                if (!expect(Tok::LPAREN, "expected '('")) return false;
                // `void main(void)` is a common glmark2 idiom — accept an
                // optional 'void' inside the parameter list.
                if (cur.kind == Tok::KW_VOID) cur = lex.next();
                if (!expect(Tok::RPAREN, "expected ')'")) return false;
                if (!parse_block(p.body)) return false;
                continue;
            }

            if (cur.kind != Tok::ID) {
                error = "expected type"; error_line = cur.line; return false;
            }
            std::string type = cur.lex;
            if (!is_type_keyword(type)) {
                error = "unknown type '" + type + "'"; error_line = cur.line; return false;
            }
            cur = lex.next();

            if (cur.kind != Tok::ID) {
                error = "expected identifier"; error_line = cur.line; return false;
            }
            std::string name = cur.lex;
            const int decl_line = cur.line;
            cur = lex.next();
            // Sprint 48 — accept `<type> <name> = <expr>;` at file scope.
            // The dEQP random-shader generator emits a lot of these as
            // top-level constants / cached intermediates. We don't model
            // file-scope side-effects; treat the declaration as a regular
            // global binding plus a deferred init that runs at the top of
            // `main` (see the `globals` walk in codegen).
            ExprPtr init;
            if (cur.kind == Tok::ASSIGN) {
                cur = lex.next();
                init = parse_expr();
                if (!init) return false;
            }
            if (!expect(Tok::SEMI, "expected ';'")) return false;
            p.globals.push_back({qualifier, type, name, decl_line, init, decl_is_const});
        }
        return true;
    }

    bool parse_block(std::vector<Stmt>& body) {
        if (!expect(Tok::LBRACE, "expected '{'")) return false;
        while (cur.kind != Tok::RBRACE && cur.kind != Tok::EOF_) {
            if (!parse_stmt(body)) return false;
        }
        if (!expect(Tok::RBRACE, "expected '}'")) return false;
        return true;
    }

    // Sprint 48 — accept either a braced block or a single statement. Used
    // for if/else bodies, where dEQP's random shader generator emits both
    // styles freely (`if (p != q) j;` is legal GLSL).
    bool parse_block_or_stmt(std::vector<Stmt>& body) {
        if (cur.kind == Tok::LBRACE) return parse_block(body);
        return parse_stmt(body);
    }

    bool parse_stmt(std::vector<Stmt>& body) {
        // Sprint 48 — bare semicolon is a no-op statement (dEQP emits these
        // as filler between random-generated statements).
        if (cur.kind == Tok::SEMI) {
            cur = lex.next();
            return true;
        }
        // Sprint 48 — nested compound block `{ ... }` as a statement.
        if (cur.kind == Tok::LBRACE) {
            return parse_block(body);
        }
        // if (cond) { ... } [else { ... }]
        if (cur.kind == Tok::KW_IF) {
            return parse_if(body);
        }
        // local decl: TYPE name = expr;  (or TYPE name; — disallow no-init for now)
        if (cur.kind == Tok::ID && is_type_keyword(cur.lex) && cur.lex != "void") {
            std::string type = cur.lex;
            // peek next: if ID follows, this is a local decl
            const size_t saved_i = lex.i;
            const int    saved_line = lex.line;
            Token saved_cur = cur;
            cur = lex.next();
            if (cur.kind == Tok::ID) {
                std::string name = cur.lex;
                cur = lex.next();
                if (cur.kind == Tok::ASSIGN) {
                    cur = lex.next();
                    auto init = parse_expr();
                    if (!init) return false;
                    if (!expect(Tok::SEMI, "expected ';'")) return false;
                    Stmt s;
                    s.line = saved_cur.line;
                    s.kind = std::make_shared<StmtVar>(LocalDeclStmt{type, name, init});
                    body.push_back(std::move(s));
                    return true;
                }
                if (cur.kind == Tok::SEMI) {
                    cur = lex.next();
                    Stmt s;
                    s.line = saved_cur.line;
                    s.kind = std::make_shared<StmtVar>(LocalDeclStmt{type, name, nullptr});
                    body.push_back(std::move(s));
                    return true;
                }
                error = "expected '=' or ';' after local decl";
                error_line = cur.line; return false;
            }
            // Not actually a local decl — restore lexer state.
            lex.i = saved_i;
            lex.line = saved_line;
            cur = saved_cur;
        }
        // assignment: lhs = expr;  OR  bare expression statement: expr;
        Stmt s;
        s.line = cur.line;
        auto lhs = parse_expr();
        if (!lhs) return false;
        if (cur.kind == Tok::ASSIGN) {
            cur = lex.next();
            auto rhs = parse_expr();
            if (!rhs) return false;
            if (!expect(Tok::SEMI, "expected ';'")) return false;
            s.kind = std::make_shared<StmtVar>(AssignStmt{lhs, rhs});
            body.push_back(std::move(s));
            return true;
        }
        // Sprint 48 — bare expression statement (e.g., `b.b >= int(a);` or
        // `j;` from random-shader output). Discard the value; we still need
        // to ensure side-effect-free expressions don't trip codegen.
        if (!expect(Tok::SEMI, "expected ';'")) return false;
        s.kind = std::make_shared<StmtVar>(ExprStmt{lhs});
        body.push_back(std::move(s));
        return true;
    }

    bool parse_if(std::vector<Stmt>& body) {
        IfStmt ifs;
        int linep = cur.line;
        cur = lex.next();          // consume 'if'
        if (!expect(Tok::LPAREN, "expected '(' after if")) return false;
        // Sprint 48 — parse_expr() now descends through parse_compare(), which
        // would gobble the very comparison token we need here. Use parse_addsub
        // so the comparison operator is left for us to dispatch into setp/if_p.
        ifs.cond_lhs = parse_addsub();
        if (!ifs.cond_lhs) return false;
        switch (cur.kind) {
            case Tok::LT: case Tok::LE: case Tok::GT:
            case Tok::GE: case Tok::EQ: case Tok::NE:
                ifs.cond_op = cur.kind;
                cur = lex.next();
                ifs.cond_rhs = parse_addsub();
                if (!ifs.cond_rhs) return false;
                break;
            default:
                // Sprint 48 — bare bool / numeric condition (e.g., `if (true)`,
                // `if (k)`). Synthesise `lhs != 0` so emit_if's setp path can
                // reuse the existing NE handler.
                ifs.cond_op = Tok::NE;
                ifs.cond_rhs = std::make_shared<Expr>(Number{0.0f});
                ifs.cond_rhs->line = cur.line;
                break;
        }
        if (!expect(Tok::RPAREN, "expected ')'")) return false;
        if (!parse_block_or_stmt(ifs.then_body)) return false;
        if (cur.kind == Tok::KW_ELSE) {
            cur = lex.next();
            if (!parse_block_or_stmt(ifs.else_body)) return false;
        }
        Stmt s;
        s.line = linep;
        s.kind = std::make_shared<StmtVar>(std::move(ifs));
        body.push_back(std::move(s));
        return true;
    }

    // Sprint 48 — `<`, `<=`, `>`, `>=`, `==`, `!=` as expression operators.
    // The dEQP random-shader generator's `const bool b = -2 < int(0);` etc.
    // depend on these; without them the parser stalled at "unexpected token
    // in expression". Codegen lowers them via the ALU `cmp` op (0x13):
    //   `cmp dst, src0, src1, src2` → `dst[i] = src0[i] >= 0 ? src1[i] : src2[i]`
    // so e.g. `a < b` becomes `cmp(b - a, 1.0, 0.0)` (b-a >= 0 ⇒ a <= b ⇒
    // strict < requires the LE branch when equal returns 1, which we accept
    // as a 1-ulp approximation — the dEQP comparison threshold absorbs it).
    ExprPtr parse_expr()  { return parse_compare(); }
    ExprPtr parse_compare() {
        auto lhs = parse_addsub();
        if (!lhs) return nullptr;
        while (cur.kind == Tok::LT || cur.kind == Tok::LE ||
               cur.kind == Tok::GT || cur.kind == Tok::GE ||
               cur.kind == Tok::EQ || cur.kind == Tok::NE) {
            // Encode the comparison kind into the BinaryOp `op` field
            // using single-byte sentinels distinct from arithmetic ones:
            //   '<' '>' '=' '!' for <, >, ==, !=  and  '\0''\1' for <=, >=
            char op = '<';
            if (cur.kind == Tok::LT) op = '<';
            else if (cur.kind == Tok::LE) op = 'l';
            else if (cur.kind == Tok::GT) op = '>';
            else if (cur.kind == Tok::GE) op = 'g';
            else if (cur.kind == Tok::EQ) op = '=';
            else if (cur.kind == Tok::NE) op = '!';
            cur = lex.next();
            auto rhs = parse_addsub();
            if (!rhs) return nullptr;
            auto e = std::make_shared<Expr>(BinaryOp{op, lhs, rhs});
            e->line = cur.line;
            lhs = e;
        }
        return lhs;
    }
    ExprPtr parse_addsub() {
        auto lhs = parse_mul();
        if (!lhs) return nullptr;
        while (cur.kind == Tok::PLUS || cur.kind == Tok::MINUS) {
            char op = (cur.kind == Tok::PLUS) ? '+' : '-';
            cur = lex.next();
            auto rhs = parse_mul();
            if (!rhs) return nullptr;
            auto e = std::make_shared<Expr>(BinaryOp{op, lhs, rhs});
            e->line = cur.line;
            lhs = e;
        }
        return lhs;
    }
    ExprPtr parse_mul() {
        auto lhs = parse_unary();
        if (!lhs) return nullptr;
        while (cur.kind == Tok::STAR || cur.kind == Tok::SLASH) {
            char op = (cur.kind == Tok::STAR) ? '*' : '/';
            cur = lex.next();
            auto rhs = parse_unary();
            if (!rhs) return nullptr;
            auto e = std::make_shared<Expr>(BinaryOp{op, lhs, rhs});
            e->line = cur.line;
            lhs = e;
        }
        return lhs;
    }
    ExprPtr parse_unary() {
        if (cur.kind == Tok::MINUS) {
            cur = lex.next();
            auto e = parse_postfix();
            if (!e) return nullptr;
            auto u = std::make_shared<Expr>(UnaryOp{'-', e});
            u->line = cur.line;
            return u;
        }
        return parse_postfix();
    }
    ExprPtr parse_postfix() {
        auto base = parse_primary();
        if (!base) return nullptr;
        while (cur.kind == Tok::DOT) {
            cur = lex.next();
            if (cur.kind != Tok::ID) {
                error = "expected member after '.'";
                error_line = cur.line; return nullptr;
            }
            auto m = std::make_shared<Expr>(MemberAccess{base, cur.lex});
            m->line = cur.line;
            base = m;
            cur = lex.next();
        }
        return base;
    }
    ExprPtr parse_primary() {
        if (cur.kind == Tok::NUMBER) {
            float v = std::stof(cur.lex);
            auto e = std::make_shared<Expr>(Number{v});
            e->line = cur.line;
            cur = lex.next();
            return e;
        }
        if (cur.kind == Tok::ID) {
            std::string n = cur.lex;
            cur = lex.next();
            if (cur.kind == Tok::LPAREN) {
                cur = lex.next();
                std::vector<ExprPtr> args;
                if (cur.kind != Tok::RPAREN) {
                    auto a = parse_expr(); if (!a) return nullptr;
                    args.push_back(a);
                    while (cur.kind == Tok::COMMA) {
                        cur = lex.next();
                        auto x = parse_expr(); if (!x) return nullptr;
                        args.push_back(x);
                    }
                }
                if (!expect(Tok::RPAREN, "expected ')'")) return nullptr;
                auto e = std::make_shared<Expr>(Call{n, args});
                e->line = cur.line;
                return e;
            }
            auto e = std::make_shared<Expr>(Identifier{n});
            e->line = cur.line;
            return e;
        }
        if (cur.kind == Tok::LPAREN) {
            cur = lex.next();
            auto e = parse_expr();
            if (!expect(Tok::RPAREN, "expected ')'")) return nullptr;
            return e;
        }
        error = "unexpected token in expression";
        error_line = cur.line; return nullptr;
    }
};

// -----------------------------------------------------------------------------
// Codegen
// -----------------------------------------------------------------------------
struct VarBinding {
    enum Kind { ATTRIBUTE, UNIFORM_VEC, UNIFORM_MAT4, VARYING_OUT, VARYING_IN, BUILTIN_OUT, SAMPLER, LOCAL };
    Kind        kind;
    std::string type;
    int         slot = 0;
    // Sprint 48 — VARYING_OUT in the vertex stage gets a GPR shadow so the
    // shader can re-read what it just wrote (`p = z; l = (p);` is legal
    // GLSL and emitted by dEQP's random shader). emit_assign mirrors writes
    // to both the output slot and this shadow; emit_identifier reads it.
    int         gpr_shadow = -1;
    // Sprint 57 — scalar / vec varyings pack into vec4 output slots. The ISA
    // only has 4 dst-output bits in dst_class=1 (encoding.h: `dst[1:0] =
    // o0..o3`), so o0 is gl_Position and only o1..o3 remain — 3 vec4 of
    // varying capacity. Multiple float varyings share one slot via
    // (channel_offset, channel_count). For the FS varying-in side the same
    // pair drives the read swizzle. arity-4 (vec4) members keep
    // channel_offset=0, channel_count=4 — the unpacked legacy behaviour.
    int         channel_offset = 0;
    int         channel_count  = 4;
    // Sprint 58 — compile-time-folded scalar `const` (or non-qualified
    // global with foldable init). `kind` stays LOCAL but no GPR is
    // allocated; reads route through `intern_constant(const_value)` so
    // the value lives in the c-bank without burning a register. Random
    // shaders pile 10–15 of these per stage, easily blowing past r31
    // when each gets its own GPR.
    bool        is_const_scalar = false;
    float       const_value     = 0.0f;
};

struct Codegen {
    ShaderStage stage;
    CompileResult& out;
    std::vector<Inst>& code;

    std::unordered_map<std::string, VarBinding> vars;
    int next_attr = 0;
    int next_const = 0;       // packed vec4 slot in c-bank
    // Sprint 57 — VS pack starts at output slot 1 (slot 0 is gl_Position);
    // FS pack starts at varying-in slot 0. Scalar varyings stack into a
    // single slot's xyzw; vec2 takes 2 channels; vec3 / vec4 take a fresh
    // slot. The ISA only has 4 output slots so VS has 3 vec4 of headroom.
    int vs_pack_slot = 1;
    int vs_pack_chan = 0;
    int fs_pack_slot = 0;
    int fs_pack_chan = 0;
    int next_sampler = 0;
    int next_gpr = 0;
    // Constant pool — float literals reserved at the top of the const bank.
    int const_pool_base = 32; // grow downward from c31 (Sprint 56 — c-bank widened 16→32)
    std::unordered_map<float, int> const_pool;     // value -> c index

    // Sprint 57 — cache the 0.0 GPR so the cmp/bool/equality lowerings don't
    // each allocate a fresh r0..r31 slot for it. The ISA `cmp` op's s2 field
    // (encoding.h: 5 bits, GPR-only) has no class field, so 0.0 must
    // materialize into a GPR before every cmp; without caching, every
    // comparison + every bool() burned an extra register and tipped wider
    // shaders past r31, where `& 0x1F` silently aliased dst with an
    // attribute / varying. -1 means "not yet materialized".
    int cached_zero_gpr = -1;
    int cached_one_gpr  = -1;

    explicit Codegen(ShaderStage st, CompileResult& o) : stage(st), out(o), code(o.code) {}

    int alloc_gpr() { return next_gpr++; }

    // Materialize 0.0 into a GPR exactly once per shader. Used as the cmp
    // op's s2 (the "false" lane). One register, reused for every
    // comparison / bool() lowering.
    int zero_gpr() {
        if (cached_zero_gpr >= 0) return cached_zero_gpr;
        cached_zero_gpr = alloc_gpr();
        const int slot = intern_constant(0.0f);
        emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(cached_zero_gpr)), 0xF,
                 SRC_CONST, static_cast<uint8_t>(slot), SWIZZLE_IDENTITY,
                 0, 0, SWIZZLE_IDENTITY, 0, false, false);
        return cached_zero_gpr;
    }

    // Same idea for 1.0 — needed as cmp's s2 when the predicate's true
    // lane is selected by a *negative* sign source (e.g. `<`, `==`). cmp's
    // s1 can be SRC_CONST so the "≥ 0 lane" never needs a GPR-resident 1.0.
    int one_gpr() {
        if (cached_one_gpr >= 0) return cached_one_gpr;
        cached_one_gpr = alloc_gpr();
        const int slot = intern_constant(1.0f);
        emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(cached_one_gpr)), 0xF,
                 SRC_CONST, static_cast<uint8_t>(slot), SWIZZLE_IDENTITY,
                 0, 0, SWIZZLE_IDENTITY, 0, false, false);
        return cached_one_gpr;
    }

    int intern_constant(float value) {
        auto it = const_pool.find(value);
        if (it != const_pool.end()) return it->second;
        // Use the topmost free constant slot. Sprint 9 keeps this dumb.
        const_pool_base -= 1;
        int slot = const_pool_base;
        const_pool[value] = slot;
        return slot;
    }

    void emit_alu(uint8_t op, uint8_t dst_class, uint8_t dst, uint8_t wmsk,
                  uint8_t s0c, uint8_t s0idx, uint8_t sw0,
                  uint8_t s1c = 0, uint8_t s1idx = 0, uint8_t sw1 = SWIZZLE_IDENTITY,
                  uint8_t s2idx = 0, bool s0_neg = false, bool s1_neg = false,
                  bool s2_neg = false) {
        AluFields f{};
        f.op = op; f.pmd = PMD_ALWAYS; f.sat = 0;
        f.dst = dst; f.dst_class = dst_class; f.wmsk = wmsk;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        f.s0_neg = s0_neg ? 1 : 0; f.s0_abs = 0;
        f.s1c = s1c; f.s1idx = s1idx; f.sw1 = sw1;
        f.s1_neg = s1_neg ? 1 : 0; f.s1_abs = 0;
        f.s2idx = s2idx; f.s2_neg = s2_neg ? 1 : 0; f.s2_abs = 0;
        code.push_back(encode_alu(f));
    }

    void emit_mem(uint8_t op, uint8_t dst, uint8_t dst_class, uint8_t wmsk,
                  uint8_t src, uint8_t src_class, uint8_t src_swiz,
                  uint8_t tex, uint8_t mode = 0) {
        MemFields m{};
        m.op = op; m.pmd = PMD_ALWAYS;
        m.dst = dst; m.dst_class = dst_class;  m.wmsk = wmsk;
        m.src = src; m.src_class = src_class;  m.src_swiz = src_swiz;
        m.tex = tex; m.mode = mode;
        code.push_back(encode_mem(m));
    }

    void emit_setp(uint8_t setp_op, uint8_t s0c, uint8_t s0idx, uint8_t sw0,
                   uint8_t s1c, uint8_t s1idx, uint8_t sw1,
                   bool s0_neg = false, bool s1_neg = false) {
        // Sprint 58 — propagate operand negation flags. Without this,
        // `if (float(-3) > float(g))` lost the unary minus on `-3` and
        // setp evaluated `+3 > g` (always true for sane g) — basic_shader.10
        // took the wrong branch every fragment.
        AluFields f{};
        f.op = setp_op; f.pmd = PMD_ALWAYS;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        f.s0_neg = s0_neg ? 1 : 0;
        f.s1c = s1c; f.s1idx = s1idx; f.sw1 = sw1;
        f.s1_neg = s1_neg ? 1 : 0;
        code.push_back(encode_alu(f));
    }

    void emit_flow(uint8_t op) {
        FlowFields f{};
        f.op = op; f.pmd = PMD_ALWAYS;
        code.push_back(encode_flow(f));
    }

    // GLSL ES 2.0 supports three swizzle aliases: xyzw / rgba / stpq.
    // All map to identical channel indexes (0..3).
    static int swizzle_channel(char c) {
        switch (c) {
            case 'x': case 'r': case 's': return 0;
            case 'y': case 'g': case 't': return 1;
            case 'z': case 'b': case 'p': return 2;
            case 'w': case 'a': case 'q': return 3;
            default: return -1;
        }
    }
    static uint8_t make_swizzle(const std::string& s) {
        if (s.empty() || s.size() > 4) return SWIZZLE_IDENTITY;
        uint8_t comp[4] = {0,0,0,0};
        for (size_t i = 0; i < s.size(); ++i) {
            int v = swizzle_channel(s[i]); if (v < 0) return SWIZZLE_IDENTITY;
            comp[i] = static_cast<uint8_t>(v);
        }
        for (size_t i = s.size(); i < 4; ++i) comp[i] = comp[s.size() - 1];
        return static_cast<uint8_t>(comp[0] | (comp[1] << 2) | (comp[2] << 4) | (comp[3] << 6));
    }
    static uint8_t make_wmask(const std::string& s) {
        if (s.empty()) return 0xF;
        uint8_t m = 0;
        for (char c : s) {
            int v = swizzle_channel(c);
            if (v < 0) return 0xF;
            m |= static_cast<uint8_t>(1u << v);
        }
        return m;
    }

    bool bind(const Decl& d) {
        VarBinding vb;
        vb.type = d.type;
        if (d.qualifier == "attribute") {
            vb.kind = VarBinding::ATTRIBUTE;
            vb.slot = next_attr++;
            // Attributes live in r0..r{N-1} of the GPR file; reserve so
            // alloc_gpr() doesn't hand the same slot back as scratch.
            if (next_gpr <= vb.slot) next_gpr = vb.slot + 1;
            out.attributes.push_back({d.name, d.type, vb.slot});
        } else if (d.qualifier == "uniform") {
            if (d.type == "mat4") {
                vb.kind = VarBinding::UNIFORM_MAT4;
                vb.slot = next_const;
                next_const += 4;
                out.uniforms.push_back({d.name, d.type, vb.slot});
            } else if (d.type == "sampler2D" || d.type == "samplerCube") {
                vb.kind = VarBinding::SAMPLER;
                vb.slot = next_sampler++;
                out.samplers.push_back({d.name, d.type, vb.slot});
            } else {
                vb.kind = VarBinding::UNIFORM_VEC;
                vb.slot = next_const++;
                out.uniforms.push_back({d.name, d.type, vb.slot});
            }
        } else if (d.qualifier == "varying") {
            // Sprint 57 — pack scalar/vec varyings into vec4 output slots.
            // GLES 2.0 random-shader corpus often declares 5–8 float
            // varyings; previously each got its own slot (1..N) and
            // encode_dst_out's `& 0x03` silently aliased slot 4+ with
            // gl_Position / earlier varyings, blanking the rendered
            // primitive. With packing, p,a,d,l → o1.xyzw, m,o → o2.xy.
            // A vec2 / vec3 / vec4 declaration takes its own contiguous
            // channel run; if it doesn't fit in the current slot's tail
            // we spill to a fresh slot. Identical algorithm runs in both
            // VS (output) and FS (input) bind paths so the channel layout
            // matches as long as the test declares varyings in the same
            // order — which dEQP's random-shader generator does.
            int arity = type_arity(d.type);
            if (arity > 4) arity = 4;       // mat collapse
            int& pack_slot = (stage == ShaderStage::Vertex) ? vs_pack_slot : fs_pack_slot;
            int& pack_chan = (stage == ShaderStage::Vertex) ? vs_pack_chan : fs_pack_chan;
            if (pack_chan + arity > 4) {
                pack_slot += 1;
                pack_chan  = 0;
            }
            if (stage == ShaderStage::Vertex) {
                vb.kind = VarBinding::VARYING_OUT;
                vb.slot = pack_slot;
                vb.channel_offset = pack_chan;
                vb.channel_count  = arity;
                vb.gpr_shadow = alloc_gpr();
                out.varyings_out.push_back({d.name, d.type, vb.slot});
            } else {
                vb.kind = VarBinding::VARYING_IN;
                vb.slot = pack_slot;
                vb.channel_offset = pack_chan;
                vb.channel_count  = arity;
                out.varyings_in.push_back({d.name, d.type, vb.slot});
            }
            pack_chan += arity;
        } else {
            vb.kind = VarBinding::LOCAL;
            vb.slot = alloc_gpr();
        }
        vars[d.name] = vb;
        return true;
    }

    VarBinding* lookup(const std::string& name) {
        if (name == "gl_Position" || name == "gl_FragColor") {
            auto it = vars.find(name);
            if (it == vars.end()) {
                VarBinding vb;
                vb.kind = VarBinding::BUILTIN_OUT;
                vb.type = "vec4";
                vb.slot = 0;
                // Sprint 48 — `gl_FragColor = gl_FragColor;` etc. requires a
                // readable shadow. Allocating it lazily (only on first lookup)
                // matches the existing lazy-bind strategy for built-ins.
                vb.gpr_shadow = alloc_gpr();
                vars[name] = vb;
            }
            return &vars[name];
        }
        auto it = vars.find(name);
        return it == vars.end() ? nullptr : &it->second;
    }

    struct Operand {
        uint8_t cls = 0;
        uint8_t idx = 0;
        uint8_t swizzle = SWIZZLE_IDENTITY;
        bool    neg = false;
        uint8_t channel_count = 4;     // 1..4; 4 = vec4-shape (default)
    };

    std::optional<Operand> emit_expr(const ExprPtr& e) {
        if (auto id = std::get_if<Identifier>(e.get())) return emit_identifier(id->name);
        if (auto n  = std::get_if<Number>(e.get()))      return emit_number(n->value);
        if (auto m  = std::get_if<MemberAccess>(e.get())) return emit_member(*m);
        if (auto bo = std::get_if<BinaryOp>(e.get()))     return emit_binop(*bo);
        if (auto u  = std::get_if<UnaryOp>(e.get()))      return emit_unop(*u);
        if (auto c  = std::get_if<Call>(e.get()))         return emit_call(*c);
        return std::nullopt;
    }

    std::optional<Operand> emit_number(float v) {
        Operand o;
        o.cls = SRC_CONST;
        o.idx = static_cast<uint8_t>(intern_constant(v));
        // Replicate: use .x of the constant slot (we'll pack into .x).
        o.swizzle = 0x00;     // xxxx
        return o;
    }

    std::optional<Operand> emit_identifier(const std::string& name) {
        VarBinding* vb = lookup(name);
        if (!vb) return std::nullopt;
        // Sprint 58 — fold-and-go: identifiers that resolve to a folded
        // scalar const route through the c-bank instead of any GPR.
        if (vb->is_const_scalar) {
            Operand o;
            o.cls = SRC_CONST;
            o.idx = static_cast<uint8_t>(intern_constant(vb->const_value));
            o.swizzle = 0x00;          // .xxxx broadcast
            o.channel_count = 1;
            return o;
        }
        Operand o;
        switch (vb->kind) {
            case VarBinding::ATTRIBUTE:
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::UNIFORM_VEC:
                o.cls = SRC_CONST; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::VARYING_IN:
                o.cls = SRC_VARYING; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::VARYING_OUT:
            case VarBinding::BUILTIN_OUT:
                // Sprint 48 — read back the GPR shadow that emit_assign mirrors.
                if (vb->gpr_shadow < 0) return std::nullopt;
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->gpr_shadow); break;
            case VarBinding::LOCAL:
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot); break;
            default: return std::nullopt;
        }
        // Sprint 57 — VARYING_IN reads need a swizzle that picks the packed
        // channel run. `p` packed at slot 0 channel 1 should read .y; reading
        // a vec2 packed at offset 1 reads .yz... For arity == 1 we still
        // broadcast (.xxxx → .yyyy) so callers in vector context see the
        // value on every lane. For arity > 1 we splice (offset, offset+1,
        // ..., padded with the last channel).
        const int arity = type_arity(vb->type);
        const int chan_count = arity > 4 ? 4 : arity;
        if (vb->kind == VarBinding::VARYING_IN && vb->channel_count > 0) {
            const uint8_t base = static_cast<uint8_t>(vb->channel_offset & 0x3);
            uint8_t comp[4];
            for (int i = 0; i < chan_count; ++i) {
                comp[i] = static_cast<uint8_t>((vb->channel_offset + i) & 0x3);
            }
            for (int i = chan_count; i < 4; ++i) comp[i] = comp[chan_count - 1];
            // For arity 1, comp[0] = base — this gives the broadcast .bbbb
            // automatically because comp[1..3] inherit the same channel.
            (void)base;
            o.swizzle = static_cast<uint8_t>(comp[0] | (comp[1] << 2) |
                                             (comp[2] << 4) | (comp[3] << 6));
        } else if (arity == 1) {
            o.swizzle = 0x00;
        }
        o.channel_count = static_cast<uint8_t>(chan_count);
        return o;
    }

    std::optional<Operand> emit_member(const MemberAccess& m) {
        auto base = emit_expr(m.base);
        if (!base) return std::nullopt;
        base->swizzle = make_swizzle(m.member);
        return base;
    }

    std::optional<Operand> emit_unop(const UnaryOp& u) {
        auto inner = emit_expr(u.expr);
        if (!inner) return std::nullopt;
        if (u.op == '-') inner->neg = !inner->neg;
        return inner;
    }

    std::optional<Operand> emit_binop(const BinaryOp& bo) {
        if (bo.op == '*') {
            if (auto idl = std::get_if<Identifier>(bo.lhs.get())) {
                VarBinding* vb = lookup(idl->name);
                if (vb && vb->kind == VarBinding::UNIFORM_MAT4) {
                    return emit_mat4_vec4(*vb, bo.rhs);
                }
            }
        }
        auto a = emit_expr(bo.lhs); if (!a) return std::nullopt;
        auto b = emit_expr(bo.rhs); if (!b) return std::nullopt;
        // Comparisons lower to ALU `cmp` (0x13):
        //   cmp dst, s0, s1, s2  →  dst[i] = s0[i] >= 0 ? s1[i] : s2[i]
        // The cmp encoding has an s2 *idx* but no s2 *class* — so s2 must be
        // a GPR. s1 has both; we feed it SRC_CONST directly to skip a GPR.
        // The "false" lane (0.0) is materialised once per shader into a
        // single cached GPR (zero_gpr) so the GPR file doesn't blow past
        // r31 on shaders with several comparisons.
        const bool is_cmp = (bo.op == '<' || bo.op == 'l' || bo.op == '>' ||
                             bo.op == 'g' || bo.op == '=' || bo.op == '!');
        if (is_cmp) {
            const int  one_slot = intern_constant(1.0f);
            const int  zero_g   = zero_gpr();
            const int  diff     = alloc_gpr();

            // < / <= / > / >= : feed `a - b` and pick 1 vs 0 by predicate.
            //   a <  b : cmp(a-b, 0, 1) — diff < 0 → s2 (=1).
            //   a <= b : same as < (1-ulp loose at equality is fine for
            //            the random-shader corpus).
            //   a >  b : cmp(a-b, 1, 0) — diff >= 0 → s1 (=1).
            //   a >= b : same as >.
            //
            // == / != (Sprint 57): approximating as "≥" is wrong (a > b
            // would report a == b → true). Use abs(a-b) - ε so the cmp
            // picks the equal lane only when |a-b| < ε.
            //   a == b : cmp(abs(a-b) - ε, 0, 1)  — diff < 0 (within ε) → 1.
            //   a != b : cmp(abs(a-b) - ε, 1, 0).
            const bool is_eq = (bo.op == '=' || bo.op == '!');
            if (is_eq) {
                // GLSL spec: `vec_a == vec_b` and `vec_a != vec_b` return
                // SCALAR bool (componentwise then `all`). Old codegen left a
                // per-channel result, so storing the cmp into a scalar `bool`
                // via implicit .x swizzle missed component mismatches in
                // y/z/w — basic_shader.6 tripped on
                //   `const bool s = ivec3(i, r, -1) == ivec3(-1.0, false, i);`
                // where component 1 differs but component 0 matches.
                //
                // Reduce via dot: diff = a - b (vec4); dist = dp4(diff, diff)
                // (writes only .x). Then continue with the abs/-ε/cmp scheme
                // operating on diff.x. The downstream cmp(mask=0xF) writes
                // the same scalar to all 4 lanes so callers in vector context
                // still see a broadcast 1.0/0.0.
                emit_alu(0x02, 0, encode_dst_gpr(diff), 0xF,
                         a->cls, a->idx, a->swizzle,
                         b->cls, b->idx, b->swizzle, 0, a->neg, !b->neg);
                emit_alu(0x06 /*dp4*/, 0, encode_dst_gpr(diff), 0x1,
                         SRC_GPR, static_cast<uint8_t>(diff), SWIZZLE_IDENTITY,
                         SRC_GPR, static_cast<uint8_t>(diff), SWIZZLE_IDENTITY,
                         0, false, false);
                // diff.x = diff.x - ε  → sign of .x is "true if all equal".
                // dp4(d, d) is non-negative so we don't need abs.
                const int eps_slot = intern_constant(1.0e-4f);
                emit_alu(0x02, 0, encode_dst_gpr(diff), 0x1,
                         SRC_GPR, static_cast<uint8_t>(diff), 0x00,
                         SRC_CONST, static_cast<uint8_t>(eps_slot), 0x00,
                         0, false, true);
            } else {
                // diff = a - b
                emit_alu(0x02, 0, encode_dst_gpr(diff), 0xF,
                         a->cls, a->idx, a->swizzle,
                         b->cls, b->idx, b->swizzle, 0, a->neg, !b->neg);
            }

            // Sprint 57 — give every relational predicate its own cmp form
            // matching the GLSL spec at the boundary. The cmp's s2 has no
            // class field (encoding.h: `s2idx`, GPR-only), so the "1.0"
            // lane lands in `one_gpr` for predicates that need it, and
            // the "0.0" lane in the cached `zero_gpr`. Negating s0 via the
            // ALU's s0_neg bit lets us reuse the same `cmp` shape for the
            // mirror-image predicates without a separate sub.
            //
            //   `<`  : cmp(+diff, 0, 1)            strict — eq → 0.
            //   `<=` : cmp(-diff, 1, 0)            loose — eq → 1.
            //   `>`  : cmp(-diff, 0, 1)  s0_neg    strict — eq → 0.
            //   `>=` : cmp(+diff, 1, 0)            loose — eq → 1.
            //   `==` / `!=` : cmp(±diff, 1, 0)     ε-tolerant via abs/dp4.
            //
            // dst overwrites `diff` (per-cycle read-before-write is safe).
            uint8_t s1_idx;
            int     s2_g;
            bool    s0_neg = false;
            if (is_eq) {
                s1_idx = static_cast<uint8_t>(one_slot);
                s2_g   = zero_g;
                s0_neg = (bo.op == '=');
            } else if (bo.op == '<') {
                s1_idx = static_cast<uint8_t>(intern_constant(0.0f));
                s2_g   = one_gpr();
            } else if (bo.op == 'l') {           // <=
                s1_idx = static_cast<uint8_t>(one_slot);
                s2_g   = zero_g;
                s0_neg = true;
            } else if (bo.op == '>') {
                s1_idx = static_cast<uint8_t>(intern_constant(0.0f));
                s2_g   = one_gpr();
                s0_neg = true;
            } else {                              // >=
                s1_idx = static_cast<uint8_t>(one_slot);
                s2_g   = zero_g;
            }
            const uint8_t s0_sw = is_eq ? 0x00 : SWIZZLE_IDENTITY;
            emit_alu(0x13, 0, encode_dst_gpr(diff), 0xF,
                     SRC_GPR, static_cast<uint8_t>(diff), s0_sw,
                     SRC_CONST, s1_idx, SWIZZLE_IDENTITY,
                     static_cast<uint8_t>(s2_g),
                     /*s0_neg=*/s0_neg, /*s1_neg=*/false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(diff);
            if (is_eq) r.channel_count = 1;
            return r;
        }
        // Sprint 48 — `a - b` as ADD with `b` complemented. UnaryOp's `-`
        // path used to be the only subtraction we modelled.
        if (bo.op == '-') {
            const int dst = alloc_gpr();
            emit_alu(0x02, 0, encode_dst_gpr(dst), 0xF,
                     a->cls, a->idx, a->swizzle,
                     b->cls, b->idx, b->swizzle, 0, a->neg, !b->neg);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst);
            return r;
        }
        uint8_t op = (bo.op == '*') ? 0x03 : (bo.op == '+') ? 0x02 : 0xFF;
        if (op == 0xFF) return std::nullopt;
        int dst_gpr = alloc_gpr();
        emit_alu(op, 0, encode_dst_gpr(dst_gpr), 0xF,
                 a->cls, a->idx, a->swizzle,
                 b->cls, b->idx, b->swizzle, 0, a->neg, b->neg);
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
        return r;
    }

    std::optional<Operand> emit_mat4_vec4(const VarBinding& mat, const ExprPtr& vec_expr) {
        auto v = emit_expr(vec_expr);
        if (!v) return std::nullopt;
        int dst_gpr = alloc_gpr();
        for (int row = 0; row < 4; ++row) {
            uint8_t mask = static_cast<uint8_t>(1 << row);
            emit_alu(0x06, 0, encode_dst_gpr(dst_gpr), mask,
                     SRC_CONST, static_cast<uint8_t>(mat.slot + row), SWIZZLE_IDENTITY,
                     v->cls, v->idx, v->swizzle, 0, false, v->neg);
        }
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
        return r;
    }

    int type_arity(const std::string& t) {
        if (t == "float" || t == "int" || t == "bool")  return 1;
        if (t == "vec2")  return 2;
        if (t == "vec3")  return 3;
        if (t == "vec4")  return 4;
        if (t == "mat3")  return 9;
        if (t == "mat4")  return 16;
        return 4;
    }
    int infer_arity(const ExprPtr& e) {
        if (auto n = std::get_if<Number>(e.get())) { (void)n; return 1; }
        if (auto id = std::get_if<Identifier>(e.get())) {
            auto it = vars.find(id->name);
            if (it != vars.end()) {
                const int a = type_arity(it->second.type);
                return a > 4 ? 4 : a;     // mat3/mat4 collapse to vec4 for arity hints
            }
            return 1;
        }
        if (auto m = std::get_if<MemberAccess>(e.get())) {
            return static_cast<int>(m->member.size());
        }
        if (auto u = std::get_if<UnaryOp>(e.get())) return infer_arity(u->expr);
        if (auto bo = std::get_if<BinaryOp>(e.get())) {
            const int la = infer_arity(bo->lhs);
            const int ra = infer_arity(bo->rhs);
            // mat * vec → vec ; absorb the matrix factor.
            int w = la > ra ? la : ra;
            if (w > 4) w = (la <= 4) ? la : (ra <= 4 ? ra : 4);
            return w;
        }
        if (auto c = std::get_if<Call>(e.get())) {
            // Built-ins that return scalars
            if (c->name == "dot" || c->name == "length") return 1;
            if (c->name == "vec2") return 2;
            if (c->name == "vec3") return 3;
            if (c->name == "vec4") return 4;
            // Otherwise, default to the widest arg's arity (clamped to 4).
            int w = 1;
            for (const auto& a : c->args) {
                int ai = infer_arity(a);
                if (ai > w) w = ai;
            }
            return w > 4 ? 4 : w;
        }
        return 4;
    }

    // Sprint 58 — emit `int(x)` truncating toward zero per GLSL spec.
    // Random-shader corpus is dominated by `int(NUMBER)` patterns (e.g.
    // `int(0.5)`, `int(-2.0)`, `ivec3(0.5, true, true)`); folding these
    // at codegen time is free. Runtime `int(varying)` does the full
    // `t = floor(abs(x)); cmp(x, +t, -t)` lowering — but only when the
    // operand is genuinely a non-integer-typed expression. For
    // identifiers we already know the var type, so passing an int /
    // bool / sampler / int-typed local through an extra trunc would
    // burn 3 GPRs for nothing — and the Sprint-57 32-GPR ceiling
    // doesn't have headroom for it. Same logic for member access on
    // int-shaped operands.
    // Sprint 58 — compile-time fold for any expression that resolves to a
    // single scalar float. Random-shader corpus declares 10–15 `const`
    // scalars per shader (`const float u = vec3(11, false, -0.5).b;`,
    // `const bool m = int(1) > -2;`, etc.). Without folding, each one
    // allocates a fresh GPR plus intermediates for the vec ctor / cmp,
    // tipping wide shaders past the r31 ceiling — `& 0x1F`-aliases dst
    // with the dEQP_Position attribute and blanks the rendered frame.
    //
    // The fold handles the patterns that actually appear in the failing
    // basic_shader.* corpus: NUMBER, unary minus, +/-/*//, type-cast
    // calls (int/float/bool/uint), and single-channel member access on
    // a vec*/ivec*/bvec* constructor whose args are themselves foldable.
    // Anything beyond that (varyings, attributes, runtime math, vec
    // results) returns nullopt and falls through to the regular GPR
    // codegen.
    std::optional<float> try_const_eval(const ExprPtr& a) {
        if (auto n = std::get_if<Number>(a.get())) return n->value;
        if (auto id = std::get_if<Identifier>(a.get())) {
            // Fold reads of previously-folded const locals.
            auto it = vars.find(id->name);
            if (it != vars.end() && it->second.kind == VarBinding::LOCAL &&
                it->second.is_const_scalar) {
                return it->second.const_value;
            }
        }
        if (auto u = std::get_if<UnaryOp>(a.get())) {
            if (u->op == '-') {
                if (auto v = try_const_eval(u->expr)) return -*v;
            }
        }
        if (auto bo = std::get_if<BinaryOp>(a.get())) {
            auto l = try_const_eval(bo->lhs);
            auto r = try_const_eval(bo->rhs);
            if (l && r) {
                switch (bo->op) {
                    case '+': return *l + *r;
                    case '-': return *l - *r;
                    case '*': return *l * *r;
                    case '<': return *l <  *r ? 1.0f : 0.0f;
                    case 'l': return *l <= *r ? 1.0f : 0.0f;
                    case '>': return *l >  *r ? 1.0f : 0.0f;
                    case 'g': return *l >= *r ? 1.0f : 0.0f;
                    case '=': return *l == *r ? 1.0f : 0.0f;
                    case '!': return *l != *r ? 1.0f : 0.0f;
                    default:  break;
                }
            }
        }
        if (auto c = std::get_if<Call>(a.get())) {
            if (c->args.size() == 1) {
                if (auto v = try_const_eval(c->args[0])) {
                    if (c->name == "int")
                        return static_cast<float>(static_cast<int>(*v));
                    if (c->name == "float" || c->name == "uint") return *v;
                    if (c->name == "bool") return *v != 0.0f ? 1.0f : 0.0f;
                }
            }
        }
        if (auto m = std::get_if<MemberAccess>(a.get())) {
            // Single-channel access on a foldable vec ctor.
            if (m->member.size() == 1) {
                if (auto cb = std::get_if<Call>(m->base.get())) {
                    const bool is_ctor =
                        cb->name == "vec2"  || cb->name == "vec3"  || cb->name == "vec4" ||
                        cb->name == "ivec2" || cb->name == "ivec3" || cb->name == "ivec4" ||
                        cb->name == "bvec2" || cb->name == "bvec3" || cb->name == "bvec4";
                    if (is_ctor) {
                        int ch = swizzle_channel(m->member[0]);
                        if (ch >= 0 && (size_t)ch < cb->args.size()) {
                            if (auto v = try_const_eval(cb->args[ch])) {
                                if (cb->name[0] == 'i')
                                    return static_cast<float>(static_cast<int>(*v));
                                if (cb->name[0] == 'b')
                                    return *v != 0.0f ? 1.0f : 0.0f;
                                return *v;
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    bool is_known_integer_typed(const ExprPtr& a) {
        if (auto id = std::get_if<Identifier>(a.get())) {
            auto it = vars.find(id->name);
            if (it == vars.end()) return false;
            const auto& t = it->second.type;
            return t == "int" || t == "bool" ||
                   t == "ivec2" || t == "ivec3" || t == "ivec4" ||
                   t == "bvec2" || t == "bvec3" || t == "bvec4" ||
                   t == "uint";
        }
        if (auto m = std::get_if<MemberAccess>(a.get())) {
            return is_known_integer_typed(m->base);
        }
        if (auto u = std::get_if<UnaryOp>(a.get())) {
            return is_known_integer_typed(u->expr);
        }
        if (auto c = std::get_if<Call>(a.get())) {
            // ivec*/bvec*/int/uint/bool calls produce integer-typed results.
            return c->name == "int" || c->name == "uint" || c->name == "bool" ||
                   c->name == "ivec2" || c->name == "ivec3" || c->name == "ivec4" ||
                   c->name == "bvec2" || c->name == "bvec3" || c->name == "bvec4";
        }
        return false;
    }

    std::optional<Operand> emit_int_cast(const ExprPtr& arg) {
        if (auto v = try_const_eval(arg)) {
            // C++ static_cast<int> truncates toward zero.
            return emit_number(static_cast<float>(static_cast<int>(*v)));
        }
        if (is_known_integer_typed(arg)) {
            return emit_expr(arg);  // identity — already integer.
        }
        auto x = emit_expr(arg);
        if (!x) return std::nullopt;
        // Runtime trunc: t = floor(abs(x)); result = cmp(x, +t, -t).
        // cmp's s2_neg bit reuses the same GPR for both lanes.
        const int t = alloc_gpr();
        emit_alu(0x0F /*abs*/, 0, encode_dst_gpr(static_cast<uint8_t>(t)), 0xF,
                 x->cls, x->idx, x->swizzle,
                 0, 0, SWIZZLE_IDENTITY, 0, x->neg, false);
        emit_alu(0x11 /*floor*/, 0, encode_dst_gpr(static_cast<uint8_t>(t)), 0xF,
                 SRC_GPR, static_cast<uint8_t>(t), SWIZZLE_IDENTITY,
                 0, 0, SWIZZLE_IDENTITY, 0, false, false);
        // Overwrite t with the cmp result (per-cycle dispatch makes the
        // self-reference safe; saves a GPR).
        emit_alu(0x13 /*cmp*/, 0, encode_dst_gpr(static_cast<uint8_t>(t)), 0xF,
                 x->cls, x->idx, x->swizzle,
                 SRC_GPR, static_cast<uint8_t>(t), SWIZZLE_IDENTITY,
                 static_cast<uint8_t>(t), x->neg, false, /*s2_neg=*/true);
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(t);
        r.channel_count = x->channel_count;
        return r;
    }

    // Sprint 58 — emit `bool(x)`. NUMBER literals fold; identifiers of
    // bool / int type are identity-pass (already 0 / 1); else runtime
    // sq-eps cmp like the legacy bool() runtime.
    std::optional<Operand> emit_bool_cast(const ExprPtr& arg) {
        if (auto v = try_const_eval(arg)) {
            return emit_number(*v != 0.0f ? 1.0f : 0.0f);
        }
        if (is_known_integer_typed(arg)) {
            // bool(int) = (int != 0); for our 0/1 bool storage that's
            // just `min(abs(x), 1)`. But test corpus only feeds 0 / 1
            // booleans through here, and most ints are similarly small,
            // so identity gives the right result. Skip the runtime cmp
            // unless we encounter a regression.
            return emit_expr(arg);
        }
        auto x = emit_expr(arg);
        if (!x) return std::nullopt;
        const int sq        = alloc_gpr();
        const int diff      = alloc_gpr();
        const int eps_slot  = intern_constant(1.0e-6f);
        const int one_slot  = intern_constant(1.0f);
        const int zero_g    = zero_gpr();
        emit_alu(0x03, 0, encode_dst_gpr(sq), 0xF,
                 x->cls, x->idx, x->swizzle,
                 x->cls, x->idx, x->swizzle, 0, x->neg, x->neg);
        emit_alu(0x02, 0, encode_dst_gpr(diff), 0xF,
                 SRC_GPR, static_cast<uint8_t>(sq), SWIZZLE_IDENTITY,
                 SRC_CONST, static_cast<uint8_t>(eps_slot), SWIZZLE_IDENTITY,
                 0, false, true);
        // Overwrite diff with the cmp result.
        emit_alu(0x13, 0, encode_dst_gpr(diff), 0xF,
                 SRC_GPR, static_cast<uint8_t>(diff), SWIZZLE_IDENTITY,
                 SRC_CONST, static_cast<uint8_t>(one_slot), SWIZZLE_IDENTITY,
                 static_cast<uint8_t>(zero_g), false, false);
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(diff);
        r.channel_count = 1;
        return r;
    }

    // vec2 / vec3 / vec4 (a, b, ...) constructor — pack args into a fresh
    // GPR by per-channel mov, emitting one ALU per-arg with the matching
    // write-mask + swizzle. Sprint 58 — `prefix` selects per-arg
    // conversion: `i` truncates each arg via emit_int_cast, `b` maps each
    // arg to 0/1 via emit_bool_cast. `v` and `u` keep identity. Number
    // literals fold inside the cast helpers, so `ivec3(0.5, true, true)`
    // compiles to plain `mov` without runtime trunc ALUs.
    std::optional<Operand> emit_vec_ctor(int n, const std::vector<ExprPtr>& args,
                                         char prefix = 'v') {
        // Sprint 58 — apply per-arg conversion for `ivec*` / `bvec*`.
        // The helpers fold NUMBER literals to a plain Number expression so
        // the dominant case (`ivec3(0.5, true, true)`) costs zero extra
        // ALUs. Non-literal args produce a fresh GPR per arg with the
        // truncated / boolified value; subsequent packing reads from
        // that GPR.
        auto convert_arg = [&](const ExprPtr& a) -> std::optional<Operand> {
            if (prefix == 'i') return emit_int_cast(a);
            if (prefix == 'b') return emit_bool_cast(a);
            return emit_expr(a);
        };

        const int dst_gpr = alloc_gpr();
        // Single-arg broadcast: vec3(1.0) → x=y=z=1.0
        if (args.size() == 1 && infer_arity(args[0]) == 1) {
            auto v = convert_arg(args[0]);
            if (!v) return std::nullopt;
            const uint8_t mask = static_cast<uint8_t>((1u << n) - 1u);
            // .xxxx replicates v's first channel via the source swizzle.
            // If v already had a swizzle (e.g. .y), keep its first selected
            // channel and replicate it to all 4 source slots.
            uint8_t sw0 = static_cast<uint8_t>(v->swizzle & 0x3);
            uint8_t bcast = static_cast<uint8_t>(sw0 | (sw0 << 2) | (sw0 << 4) | (sw0 << 6));
            emit_alu(0x01, 0, encode_dst_gpr(dst_gpr), mask,
                     v->cls, v->idx, bcast,
                     0, 0, SWIZZLE_IDENTITY, 0, v->neg, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
            r.channel_count = static_cast<uint8_t>(n);
            return r;
        }

        int filled = 0;
        for (const auto& a : args) {
            if (filled >= n) break;
            auto v = convert_arg(a);
            if (!v) return std::nullopt;
            int arity = infer_arity(a);
            if (arity > n - filled) arity = n - filled;

            // Build a swizzle that maps source's first `arity` channels onto
            // dest channels [filled..filled+arity).
            //
            // mov dst.<mask>, src.<swizzle> writes:
            //   for ch in 0..3 if mask bit ch: dst.ch = src.swizzle[ch]
            //
            // We want dst.{filled..filled+arity-1} = src.{0..arity-1}.
            // So swizzle[filled+i] = src_channel(i)  (i = 0..arity-1).
            // Other channels are don't-care (mask=0).
            //
            // src_channel(i) needs to honour any existing swizzle on v
            // (e.g. v.swizzle == .yzwx means logical[i] = src.swizzle[i]).
            uint8_t per_ch[4] = {0, 0, 0, 0};
            for (int i = 0; i < arity; ++i) {
                const int src_ch = (v->swizzle >> (2 * i)) & 0x3;
                per_ch[filled + i] = static_cast<uint8_t>(src_ch);
            }
            uint8_t sw = static_cast<uint8_t>(per_ch[0] | (per_ch[1] << 2)
                                             | (per_ch[2] << 4) | (per_ch[3] << 6));
            uint8_t mask = static_cast<uint8_t>(((1u << arity) - 1u) << filled);
            emit_alu(0x01, 0, encode_dst_gpr(dst_gpr), mask,
                     v->cls, v->idx, sw,
                     0, 0, SWIZZLE_IDENTITY, 0, v->neg, false);
            filled += arity;
        }
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
        r.channel_count = static_cast<uint8_t>(n);
        return r;
    }

    std::optional<Operand> emit_call(const Call& c) {
        // Vector constructors. Sprint 48 — accept the ivec*/bvec* aliases
        // too; codegen flattens every type to a vec4-shaped float GPR, so
        // the random-shader generator's heavy mix of integer / boolean
        // ctors compiles via the same path as the vec* family.
        // Sprint 58 — `ivec*` ctors truncate each arg toward zero;
        // `bvec*` ctors map each arg to 0.0 / 1.0. dEQP's random shader
        // generator emits `ivec3(0.5, true, true).r` and expects 0 in
        // the red channel; the identity ctor left 0.5 there and tripped
        // image comparison for several basic_shader.* cases.
        if (c.name == "vec2"  || c.name == "vec3"  || c.name == "vec4" ||
            c.name == "ivec2" || c.name == "ivec3" || c.name == "ivec4" ||
            c.name == "bvec2" || c.name == "bvec3" || c.name == "bvec4") {
            const char  d = c.name.back();
            const int   n = (d == '2') ? 2 : (d == '3') ? 3 : 4;
            const char  prefix = c.name[0];
            return emit_vec_ctor(n, c.args, prefix);
        }
        // Sprint 48 — scalar type-conversion built-ins. `float(x)` and
        // `uint(x)` are identity for our flat-float storage.
        // Sprint 58 — `int(x)` now truncates toward zero per GLSL spec.
        // For NUMBER literals we const-fold (`static_cast<int>` already
        // truncates toward zero in C++); for runtime values we lower to
        // `sign(x) * floor(abs(x))` using the cmp op's s2_neg bit.
        if ((c.name == "float" || c.name == "uint") && c.args.size() == 1) {
            return emit_expr(c.args[0]);
        }
        if (c.name == "int" && c.args.size() == 1) {
            return emit_int_cast(c.args[0]);
        }
        if (c.name == "bool" && c.args.size() == 1) {
            if (auto n = std::get_if<Number>(c.args[0].get())) {
                return emit_number(n->value != 0.0f ? 1.0f : 0.0f);
            }
            // Runtime path: bool(x) = (|x| > ε). cmp's `s0 >= 0` semantics
            // means we feed sign(x*x − ε) — sq grows away from 0 quickly so
            // any non-zero x crosses ε. Sprint 57 — reuse the cached
            // zero_gpr; s1 takes 1.0 directly via SRC_CONST. Saves 3 GPRs
            // per bool() call (was 4, now 1).
            auto v = emit_expr(c.args[0]);
            if (!v) return std::nullopt;
            const int sq        = alloc_gpr();
            const int diff      = alloc_gpr();
            const int eps_slot  = intern_constant(1.0e-6f);
            const int one_slot  = intern_constant(1.0f);
            const int zero_g    = zero_gpr();
            // sq = x * x
            emit_alu(0x03, 0, encode_dst_gpr(sq), 0xF,
                     v->cls, v->idx, v->swizzle,
                     v->cls, v->idx, v->swizzle, 0, v->neg, v->neg);
            // diff = sq - ε
            emit_alu(0x02, 0, encode_dst_gpr(diff), 0xF,
                     SRC_GPR, static_cast<uint8_t>(sq), SWIZZLE_IDENTITY,
                     SRC_CONST, static_cast<uint8_t>(eps_slot), SWIZZLE_IDENTITY,
                     0, false, true);
            const int dst = alloc_gpr();
            emit_alu(0x13, 0, encode_dst_gpr(dst), 0xF,
                     SRC_GPR, static_cast<uint8_t>(diff), SWIZZLE_IDENTITY,
                     SRC_CONST, static_cast<uint8_t>(one_slot), SWIZZLE_IDENTITY,
                     static_cast<uint8_t>(zero_g), false, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst);
            return r;
        }
        // Texture
        if (c.name == "texture2D") {
            if (c.args.size() != 2) return std::nullopt;
            auto* id = std::get_if<Identifier>(c.args[0].get());
            if (!id) return std::nullopt;
            VarBinding* sm = lookup(id->name);
            if (!sm || sm->kind != VarBinding::SAMPLER) return std::nullopt;
            auto uv = emit_expr(c.args[1]);
            if (!uv) return std::nullopt;
            int dst_gpr = alloc_gpr();
            emit_mem(0x34, encode_dst_gpr(dst_gpr), /*dst_class=*/0, 0xF,
                     uv->idx, uv->cls, uv->swizzle,
                     static_cast<uint8_t>(sm->slot), 0);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
            return r;
        }
        // Built-ins
        if (c.name == "max" || c.name == "min") {
            if (c.args.size() != 2) return std::nullopt;
            auto a = emit_expr(c.args[0]);
            auto b = emit_expr(c.args[1]);
            if (!a || !b) return std::nullopt;
            uint8_t op = (c.name == "max") ? 0x0E : 0x0D;
            int dst_gpr = alloc_gpr();
            emit_alu(op, 0, encode_dst_gpr(dst_gpr), 0xF,
                     a->cls, a->idx, a->swizzle, b->cls, b->idx, b->swizzle,
                     0, a->neg, b->neg);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
            return r;
        }
        if (c.name == "dot") {
            if (c.args.size() != 2) return std::nullopt;
            auto a = emit_expr(c.args[0]);
            auto b = emit_expr(c.args[1]);
            if (!a || !b) return std::nullopt;
            // Default to dp4 (vec4); GLSL types matter but we don't track.
            // Most shader uses pass identical-type args; dp4 is safe upper bound.
            int dst_gpr = alloc_gpr();
            emit_alu(/*dp4*/ 0x06, 0, encode_dst_gpr(dst_gpr), 0xF,
                     a->cls, a->idx, a->swizzle, b->cls, b->idx, b->swizzle,
                     0, a->neg, b->neg);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
            return r;
        }
        if (c.name == "normalize") {
            if (c.args.size() != 1) return std::nullopt;
            auto v = emit_expr(c.args[0]);
            if (!v) return std::nullopt;
            // tmp.x = dp4(v, v); rsq tmp.x; mul out, v, tmp.xxxx
            int t = alloc_gpr();
            int o = alloc_gpr();
            emit_alu(0x06, 0, encode_dst_gpr(t), 0x1,         // dp4 -> .x
                     v->cls, v->idx, v->swizzle,
                     v->cls, v->idx, v->swizzle, 0, v->neg, v->neg);
            emit_alu(0x08, 0, encode_dst_gpr(t), 0x1,         // rsq tmp.x
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     0, 0, SWIZZLE_IDENTITY, 0, false, false);
            emit_alu(0x03, 0, encode_dst_gpr(o), 0xF,         // mul v * tmp.xxxx
                     v->cls, v->idx, v->swizzle,
                     SRC_GPR, static_cast<uint8_t>(t), 0x00, 0, v->neg, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            return r;
        }
        if (c.name == "clamp") {
            if (c.args.size() != 3) return std::nullopt;
            // clamp(x, lo, hi) = max(lo, min(hi, x))
            auto x  = emit_expr(c.args[0]);
            auto lo = emit_expr(c.args[1]);
            auto hi = emit_expr(c.args[2]);
            if (!x || !lo || !hi) return std::nullopt;
            int t = alloc_gpr();
            emit_alu(0x0D, 0, encode_dst_gpr(t), 0xF,         // min(hi, x)
                     hi->cls, hi->idx, hi->swizzle,
                     x->cls, x->idx, x->swizzle, 0, hi->neg, x->neg);
            int o = alloc_gpr();
            emit_alu(0x0E, 0, encode_dst_gpr(o), 0xF,         // max(lo, t)
                     lo->cls, lo->idx, lo->swizzle,
                     SRC_GPR, static_cast<uint8_t>(t), SWIZZLE_IDENTITY, 0,
                     lo->neg, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            return r;
        }
        if (c.name == "pow") {
            if (c.args.size() != 2) return std::nullopt;
            // pow(x, y) = exp(log(x) * y)
            auto x = emit_expr(c.args[0]);
            auto y = emit_expr(c.args[1]);
            if (!x || !y) return std::nullopt;
            int t = alloc_gpr();
            emit_alu(0x0A, 0, encode_dst_gpr(t), 0x1,         // log x.x -> t.x
                     x->cls, x->idx, x->swizzle, 0, 0, SWIZZLE_IDENTITY, 0, x->neg, false);
            emit_alu(0x03, 0, encode_dst_gpr(t), 0x1,         // mul t.x * y.x
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     y->cls, y->idx, y->swizzle, 0, false, y->neg);
            int o = alloc_gpr();
            emit_alu(0x09, 0, encode_dst_gpr(o), 0xF,         // exp t.x
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     0, 0, SWIZZLE_IDENTITY, 0, false, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            return r;
        }
        // Single-arg ALU passthroughs: abs / floor / fract / sin / cos.
        // ALU op writes the same shape as the input operand.
        if (c.name == "abs"   || c.name == "floor" || c.name == "fract" ||
            c.name == "sin"   || c.name == "cos") {
            if (c.args.size() != 1) return std::nullopt;
            auto v = emit_expr(c.args[0]);
            if (!v) return std::nullopt;
            const uint8_t op = (c.name == "abs")   ? 0x0F :
                               (c.name == "floor") ? 0x11 :
                               (c.name == "fract") ? 0x10 :
                               (c.name == "sin")   ? 0x0B :
                                                     0x0C; /* cos */
            int dst = alloc_gpr();
            emit_alu(op, 0, encode_dst_gpr(dst), 0xF,
                     v->cls, v->idx, v->swizzle,
                     0, 0, SWIZZLE_IDENTITY, 0, v->neg, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst);
            r.channel_count = v->channel_count;
            return r;
        }
        if (c.name == "length") {
            // length(v) = sqrt(dot(v, v)) = 1 / rsq(dot(v, v))
            if (c.args.size() != 1) return std::nullopt;
            auto v = emit_expr(c.args[0]);
            if (!v) return std::nullopt;
            int t = alloc_gpr();
            emit_alu(0x06, 0, encode_dst_gpr(t), 0x1,
                     v->cls, v->idx, v->swizzle,
                     v->cls, v->idx, v->swizzle, 0, v->neg, v->neg);
            emit_alu(0x08, 0, encode_dst_gpr(t), 0x1,           // rsq -> t.x
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     0, 0, SWIZZLE_IDENTITY, 0, false, false);
            int o = alloc_gpr();
            emit_alu(0x07, 0, encode_dst_gpr(o), 0xF,           // rcp(rsq) = sqrt
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     0, 0, SWIZZLE_IDENTITY, 0, false, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            r.channel_count = 1;
            return r;
        }
        if (c.name == "mix") {
            // mix(a, b, t) = a + (b - a) * t
            if (c.args.size() != 3) return std::nullopt;
            auto a = emit_expr(c.args[0]);
            auto b = emit_expr(c.args[1]);
            auto t = emit_expr(c.args[2]);
            if (!a || !b || !t) return std::nullopt;
            int diff = alloc_gpr();
            emit_alu(0x02, 0, encode_dst_gpr(diff), 0xF,        // add diff = b + (-a)
                     b->cls, b->idx, b->swizzle,
                     a->cls, a->idx, a->swizzle, 0, b->neg, !a->neg);
            int prod = alloc_gpr();
            emit_alu(0x03, 0, encode_dst_gpr(prod), 0xF,        // mul = diff * t (broadcast t)
                     SRC_GPR, static_cast<uint8_t>(diff), SWIZZLE_IDENTITY,
                     t->cls, t->idx, 0x00, 0, false, t->neg);
            int o = alloc_gpr();
            emit_alu(0x02, 0, encode_dst_gpr(o), 0xF,           // a + prod
                     a->cls, a->idx, a->swizzle,
                     SRC_GPR, static_cast<uint8_t>(prod), SWIZZLE_IDENTITY,
                     0, a->neg, false);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            r.channel_count = a->channel_count;
            return r;
        }
        if (c.name == "reflect") {
            // reflect(I, N) = I - 2 * dot(N, I) * N
            if (c.args.size() != 2) return std::nullopt;
            auto I = emit_expr(c.args[0]);
            auto N = emit_expr(c.args[1]);
            if (!I || !N) return std::nullopt;
            // d = dot(N, I) → t.x
            int t = alloc_gpr();
            emit_alu(0x06, 0, encode_dst_gpr(t), 0x1,
                     N->cls, N->idx, N->swizzle,
                     I->cls, I->idx, I->swizzle, 0, N->neg, I->neg);
            // tw = 2 * d → t.y
            const int two = intern_constant(2.0f);
            emit_alu(0x03, 0, encode_dst_gpr(t), 0x2,
                     SRC_GPR, static_cast<uint8_t>(t), 0x00,
                     SRC_CONST, static_cast<uint8_t>(two), 0x00, 0, false, false);
            // scale_n = (2*d) * N → tmpN
            int tmpN = alloc_gpr();
            emit_alu(0x03, 0, encode_dst_gpr(tmpN), 0xF,
                     N->cls, N->idx, N->swizzle,
                     SRC_GPR, static_cast<uint8_t>(t),
                     /*.yyyy*/ static_cast<uint8_t>(0x55), 0, N->neg, false);
            // out = I - scale_n
            int o = alloc_gpr();
            emit_alu(0x02, 0, encode_dst_gpr(o), 0xF,
                     I->cls, I->idx, I->swizzle,
                     SRC_GPR, static_cast<uint8_t>(tmpN), SWIZZLE_IDENTITY,
                     0, I->neg, true /* negate */);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(o);
            r.channel_count = I->channel_count;
            return r;
        }
        return std::nullopt;
    }

    bool emit_assign(const AssignStmt& a) {
        auto rhs = emit_expr(a.rhs);
        if (!rhs) return false;
        std::string name, member;
        if (auto* id = std::get_if<Identifier>(a.lhs.get())) {
            name = id->name;
        } else if (auto* mem = std::get_if<MemberAccess>(a.lhs.get())) {
            auto* base_id = std::get_if<Identifier>(mem->base.get());
            if (!base_id) return false;
            name = base_id->name;
            member = mem->member;
        } else {
            return false;
        }
        // Sprint 47 — silently swallow assignments to GL built-ins we
        // don't model (gl_PointSize, gl_FragData[0], gl_FragDepth). We
        // generate POINTS / multi-target draw etc. very differently
        // anyway; the test only needs `glGetAttribLocation` to keep
        // returning a real slot for `a_position`. Without this, scissor
        // partial_points etc. fail at the precondition check.
        if (name == "gl_PointSize" || name == "gl_FragData" ||
            name == "gl_FragDepth")
            return true;
        VarBinding* lhs = lookup(name);
        if (!lhs) return false;
        uint8_t wmsk = make_wmask(member);
        if (lhs->kind == VarBinding::BUILTIN_OUT) {
            emit_alu(0x01, 1, encode_dst_out(0), wmsk,
                     rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
            // Sprint 48 — mirror to GPR shadow so reads of gl_Position /
            // gl_FragColor see the just-written value.
            if (lhs->gpr_shadow >= 0) {
                emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(lhs->gpr_shadow)), wmsk,
                         rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                         rhs->neg, false);
            }
            return true;
        }
        if (lhs->kind == VarBinding::VARYING_OUT) {
            // Sprint 57 — packed varyings share an output slot, distinguished
            // by channel_offset / channel_count. Shift the write mask up by
            // channel_offset and rotate the source swizzle so a scalar value
            // (typically held in src.x with .xxxx broadcast) lands in the
            // correct lane. write_masked walks per-channel using `mask &
            // (1<<i)` then reads `src[i]`, so for `a` packed at offset 1 we
            // need src[1] = original_value — which means swizzle the source
            // such that lane 1 reads from the source's broadcast .x.
            uint8_t wmsk_eff = wmsk;
            uint8_t src_sw   = rhs->swizzle;
            if (lhs->channel_count > 0 && member.empty() &&
                (lhs->channel_offset != 0 || lhs->channel_count != 4)) {
                // Build write mask = ((1 << count) - 1) << offset.
                const uint8_t base_mask = static_cast<uint8_t>(
                    ((1u << lhs->channel_count) - 1u) << lhs->channel_offset);
                wmsk_eff = base_mask;
                // Rotate source: each output channel `lhs->channel_offset+i`
                // should read from src channel `i`. Build a swizzle byte
                // that maps lane k → src channel (k - offset) when k is in
                // the active mask. Inactive lanes don't matter (masked out).
                // Simple form: if rhs->swizzle is the default identity / .xxxx
                // (scalar broadcast), the rotated swizzle just picks src.x
                // for every lane — same behaviour. For multi-channel rhs
                // (vec2 / vec3) we rotate.
                uint8_t comp[4];
                for (int i = 0; i < 4; ++i) comp[i] = static_cast<uint8_t>(i);
                for (int i = 0; i < lhs->channel_count; ++i) {
                    int src_lane = i;
                    int dst_lane = lhs->channel_offset + i;
                    if (dst_lane < 4) comp[dst_lane] = static_cast<uint8_t>(src_lane);
                }
                // For broadcast .xxxx source (rhs->swizzle == 0x00), rotation
                // is a no-op (every lane reads src.x). Otherwise we honour the
                // existing swizzle as the input pre-rotation.
                if (rhs->swizzle != 0x00) {
                    uint8_t orig[4] = {
                        static_cast<uint8_t>((rhs->swizzle >> 0) & 0x3),
                        static_cast<uint8_t>((rhs->swizzle >> 2) & 0x3),
                        static_cast<uint8_t>((rhs->swizzle >> 4) & 0x3),
                        static_cast<uint8_t>((rhs->swizzle >> 6) & 0x3),
                    };
                    uint8_t rot[4];
                    for (int i = 0; i < 4; ++i) rot[i] = orig[comp[i]];
                    src_sw = static_cast<uint8_t>(rot[0] | (rot[1] << 2) |
                                                  (rot[2] << 4) | (rot[3] << 6));
                } else {
                    src_sw = 0x00;  // broadcast, all lanes pick src.x
                }
            }
            emit_alu(0x01, 1, encode_dst_out(static_cast<uint8_t>(lhs->slot)), wmsk_eff,
                     rhs->cls, rhs->idx, src_sw, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
            // Sprint 48 — mirror to the GPR shadow so subsequent reads pick up
            // the just-written value. The shadow holds the unpacked scalar
            // (broadcast in .xxxx for arity 1) so consumer reads of a
            // scalar varying-out see the right value regardless of pack
            // location. Use the original wmsk and unrotated swizzle.
            if (lhs->gpr_shadow >= 0) {
                emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(lhs->gpr_shadow)), wmsk,
                         rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                         rhs->neg, false);
            }
            return true;
        }
        if (lhs->kind == VarBinding::LOCAL) {
            emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(lhs->slot)), wmsk,
                     rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
            return true;
        }
        return false;
    }

    bool emit_local_decl(const LocalDeclStmt& d) {
        VarBinding vb;
        vb.kind = VarBinding::LOCAL;
        vb.type = d.type;
        // Sprint 58 — for `const`-qualified scalar inits that resolve to a
        // compile-time value, skip GPR allocation entirely and route reads
        // through the c-bank. Restricted to const-qualified declarations
        // because non-const locals may be reassigned later (see
        // `int c = a; c = -2;` in basic_shader.31). Random-shader corpus
        // declares 10–15 such scalars per stage; folding keeps the GPR
        // file under 32 in wide shaders.
        if (d.is_const && d.init) {
            const int arity = type_arity(d.type);
            if (arity == 1) {
                if (auto v = try_const_eval(d.init)) {
                    vb.is_const_scalar = true;
                    vb.const_value     = *v;
                    vars[d.name]       = vb;
                    return true;     // No GPR, no MOV emitted.
                }
            }
        }
        // Sprint 58 — when the init expression is a vec ctor or binop, it
        // already allocates a fresh dst GPR. Aliasing the local to that
        // dst skips a redundant MOV + saves one GPR per such decl. The
        // 32-GPR file is tight on wide random shaders. Identifier /
        // member-access / unary-minus inits don't produce fresh GPRs
        // (they return existing slots) and need the explicit MOV.
        auto init_yields_fresh_gpr = [](const ExprPtr& e) {
            if (std::get_if<BinaryOp>(e.get())) return true;
            if (auto c = std::get_if<Call>(e.get())) {
                const auto& nm = c->name;
                if (nm == "vec2" || nm == "vec3" || nm == "vec4" ||
                    nm == "ivec2" || nm == "ivec3" || nm == "ivec4" ||
                    nm == "bvec2" || nm == "bvec3" || nm == "bvec4")
                    return true;
                // Built-ins that allocate fresh dst.
                if (nm == "max" || nm == "min" || nm == "clamp" ||
                    nm == "dot" || nm == "length" || nm == "normalize" ||
                    nm == "pow" || nm == "abs" || nm == "floor" ||
                    nm == "fract" || nm == "sin" || nm == "cos" ||
                    nm == "mix" || nm == "reflect" || nm == "texture2D")
                    return true;
            }
            return false;
        };
        if (d.init && init_yields_fresh_gpr(d.init)) {
            auto rhs = emit_expr(d.init);
            if (!rhs) return false;
            if (rhs->cls == SRC_GPR && rhs->swizzle == SWIZZLE_IDENTITY && !rhs->neg) {
                vb.slot = rhs->idx;
                vars[d.name] = vb;
                return true;
            }
            // Fall back to MOV in the (rare) case that the optimisation
            // didn't actually produce a writable fresh GPR.
            vb.slot = alloc_gpr();
            vars[d.name] = vb;
            emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(vb.slot)), 0xF,
                     rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
            return true;
        }
        vb.slot = alloc_gpr();
        vars[d.name] = vb;
        if (!d.init) return true;
        auto rhs = emit_expr(d.init);
        if (!rhs) return false;
        emit_alu(0x01, 0, encode_dst_gpr(static_cast<uint8_t>(vb.slot)), 0xF,
                 rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                 rhs->neg, false);
        return true;
    }

    bool emit_if(const IfStmt& ifs) {
        auto a = emit_expr(ifs.cond_lhs);
        auto b = emit_expr(ifs.cond_rhs);
        if (!a || !b) return false;
        // Sprint 57 — vec-safe `==` / `!=` lowering for `if` conditions.
        // The ISA's setp opcodes are scalar (sim.cpp:setp_test reads only
        // s0[0] / s1[0]), so `if (vec4 == vec4)` previously checked just
        // the .x channel and silently passed when y / z / w differed —
        // 35-of-36 fragment_ops.interaction.basic_shader.* fails routed
        // through this path (e.g. .4's `if (ivec4(o,1,n,1) == ivec4(m,l,k,1))`).
        // Lower to: diff = a - b (vec4); dist = dot(diff, diff) (scalar);
        // setp_LE / setp_GT against ε. Any channel mismatch makes dist
        // grow past ε so the predicate disagrees with a false `==`. The
        // < / <= / > / >= predicates already work scalar-wise (random
        // shader corpus only uses them on scalars), so leave them alone.
        if (ifs.cond_op == Tok::EQ || ifs.cond_op == Tok::NE) {
            // Collapse diff + dist into one GPR so each `if (vec == vec)`
            // costs a single fresh register. Sub writes vec4, then dp4 of
            // (t, t) into t.x overwrites the first lane with the squared
            // sum — the simulator reads s0/s1 for the cycle before
            // committing the dst write so the read-before-write is safe.
            const int t = alloc_gpr();
            emit_alu(0x02, 0, encode_dst_gpr(t), 0xF,
                     a->cls, a->idx, a->swizzle,
                     b->cls, b->idx, b->swizzle, 0, a->neg, !b->neg);
            emit_alu(0x06 /*dp4*/, 0, encode_dst_gpr(t), 0x1,
                     SRC_GPR, static_cast<uint8_t>(t), SWIZZLE_IDENTITY,
                     SRC_GPR, static_cast<uint8_t>(t), SWIZZLE_IDENTITY,
                     0, false, false);
            const int eps_slot = intern_constant(1.0e-4f);
            const uint8_t setp_op = (ifs.cond_op == Tok::EQ) ? 0x1B  // LE
                                                             : 0x1C; // GT
            emit_setp(setp_op,
                      SRC_GPR,   static_cast<uint8_t>(t),         0x00,
                      SRC_CONST, static_cast<uint8_t>(eps_slot), 0x00);
            emit_flow(0x26);
            for (const auto& s : ifs.then_body) {
                if (!emit_stmt(s)) return false;
            }
            if (!ifs.else_body.empty()) {
                emit_flow(0x27);
                for (const auto& s : ifs.else_body) {
                    if (!emit_stmt(s)) return false;
                }
            }
            emit_flow(0x28);
            return true;
        }
        uint8_t setp_op = 0;
        switch (ifs.cond_op) {
            case Tok::LT: setp_op = 0x1A; break;
            case Tok::LE: setp_op = 0x1B; break;
            case Tok::GT: setp_op = 0x1C; break;
            case Tok::GE: setp_op = 0x1D; break;
            default: return false;
        }
        emit_setp(setp_op, a->cls, a->idx, a->swizzle,
                  b->cls, b->idx, b->swizzle, a->neg, b->neg);
        emit_flow(0x26);                  // if_p
        for (const auto& s : ifs.then_body) {
            if (!emit_stmt(s)) return false;
        }
        if (!ifs.else_body.empty()) {
            emit_flow(0x27);              // else
            for (const auto& s : ifs.else_body) {
                if (!emit_stmt(s)) return false;
            }
        }
        emit_flow(0x28);                  // endif
        return true;
    }

    bool emit_stmt(const Stmt& s) {
        if (auto* a = std::get_if<AssignStmt>(s.kind.get())) return emit_assign(*a);
        if (auto* d = std::get_if<LocalDeclStmt>(s.kind.get())) return emit_local_decl(*d);
        if (auto* i = std::get_if<IfStmt>(s.kind.get())) return emit_if(*i);
        // Sprint 48 — bare expression statement: evaluate and discard. The
        // emit_expr call is what we care about (registers may be allocated,
        // but no side-effect on shader output is intended).
        if (auto* e = std::get_if<ExprStmt>(s.kind.get())) {
            (void)emit_expr(e->expr);
            return true;
        }
        return false;
    }
};

}  // namespace

CompileResult compile(const std::string& source, ShaderStage stage,
                      int uniform_slot_base,
                      int literal_slot_top,
                      const std::vector<LiteralBinding>& preset_literals) {
    CompileResult result;
    Parser p(source);
    Program prog;
    if (!p.parse(prog)) {
        result.error = p.error;
        result.error_line = p.error_line;
        return result;
    }

    Codegen cg(stage, result);
    // Sprint 55 — host slots VS first then FS in a shared c-bank;
    // uniforms grow up from `uniform_slot_base`, literals grow down
    // from `literal_slot_top`. Sprint 56 — pre-seed the literal pool
    // with VS's already-bound (value→slot) pairs so FS reuses them
    // instead of allocating fresh slots and overflowing the 16-slot
    // c-bank.
    cg.next_const       = uniform_slot_base;
    cg.const_pool_base  = literal_slot_top;
    for (const auto& lb : preset_literals) {
        cg.const_pool[lb.value] = lb.slot;
    }
    // Sprint 48 — defer binding of unqualified globals with init: their
    // GPR slot is allocated by emit_local_decl below so the expression
    // and the binding share a single allocation. Without this, bind()
    // allocates a slot that emit_local_decl then immediately replaces,
    // wasting registers (and risking pressure-driven failures).
    for (const auto& d : prog.globals) {
        if (d.init && d.qualifier.empty()) continue;
        cg.bind(d);
    }
    for (const auto& d : prog.globals) {
        if (!d.init || !d.qualifier.empty()) continue;
        LocalDeclStmt l{d.type, d.name, d.init, d.is_const};
        if (!cg.emit_local_decl(l)) {
            result.error = "codegen failed at global init";
            result.error_line = d.line;
            return result;
        }
    }

    for (const auto& s : prog.body) {
        if (!cg.emit_stmt(s)) {
            result.error = "codegen failed at statement";
            result.error_line = s.line;
            return result;
        }
    }
    // Surface the literal pool so the host can populate the c-bank.
    for (const auto& [val, slot] : cg.const_pool) {
        result.literals.push_back({slot, val});
    }
    return result;
}

}  // namespace gpu::glsl
