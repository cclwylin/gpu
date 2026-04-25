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
};

// Statements: assignment, local declaration, if/else.
struct AssignStmt { ExprPtr lhs; ExprPtr rhs; };
struct LocalDeclStmt { std::string type; std::string name; ExprPtr init; };
struct IfStmt;     // forward
using StmtVar = std::variant<AssignStmt, LocalDeclStmt, IfStmt>;
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
        return s == "void" || s == "float" || s == "vec2" || s == "vec3" ||
               s == "vec4" || s == "mat2" || s == "mat3" || s == "mat4" ||
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
            cur = lex.next();
            if (!expect(Tok::SEMI, "expected ';'")) return false;
            p.globals.push_back({qualifier, type, name, cur.line});
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

    bool parse_stmt(std::vector<Stmt>& body) {
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
        // assignment: lhs = expr;
        Stmt s;
        s.line = cur.line;
        auto lhs = parse_unary();
        if (!lhs) return false;
        if (!expect(Tok::ASSIGN, "expected '='")) return false;
        auto rhs = parse_expr();
        if (!rhs) return false;
        if (!expect(Tok::SEMI, "expected ';'")) return false;
        s.kind = std::make_shared<StmtVar>(AssignStmt{lhs, rhs});
        body.push_back(std::move(s));
        return true;
    }

    bool parse_if(std::vector<Stmt>& body) {
        IfStmt ifs;
        int linep = cur.line;
        cur = lex.next();          // consume 'if'
        if (!expect(Tok::LPAREN, "expected '(' after if")) return false;
        ifs.cond_lhs = parse_expr();
        if (!ifs.cond_lhs) return false;
        // Condition operator must be a comparison.
        switch (cur.kind) {
            case Tok::LT: case Tok::LE: case Tok::GT:
            case Tok::GE: case Tok::EQ: case Tok::NE:
                ifs.cond_op = cur.kind;
                cur = lex.next();
                break;
            default:
                error = "if condition must be 'a OP b' (== != < <= > >=)";
                error_line = cur.line; return false;
        }
        ifs.cond_rhs = parse_expr();
        if (!ifs.cond_rhs) return false;
        if (!expect(Tok::RPAREN, "expected ')'")) return false;
        if (!parse_block(ifs.then_body)) return false;
        if (cur.kind == Tok::KW_ELSE) {
            cur = lex.next();
            if (!parse_block(ifs.else_body)) return false;
        }
        Stmt s;
        s.line = linep;
        s.kind = std::make_shared<StmtVar>(std::move(ifs));
        body.push_back(std::move(s));
        return true;
    }

    ExprPtr parse_expr()  { return parse_addsub(); }
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
};

struct Codegen {
    ShaderStage stage;
    CompileResult& out;
    std::vector<Inst>& code;

    std::unordered_map<std::string, VarBinding> vars;
    int next_attr = 0;
    int next_const = 0;       // packed vec4 slot in c-bank
    int next_vsout = 1;
    int next_fsin = 0;
    int next_sampler = 0;
    int next_gpr = 0;
    // Constant pool — float literals reserved at the top of the const bank.
    int const_pool_base = 16; // grow downward from c15 (Sprint 9: simple)
    std::unordered_map<float, int> const_pool;     // value -> c index

    explicit Codegen(ShaderStage st, CompileResult& o) : stage(st), out(o), code(o.code) {}

    int alloc_gpr() { return next_gpr++; }

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
                  uint8_t s2idx = 0, bool s0_neg = false, bool s1_neg = false) {
        AluFields f{};
        f.op = op; f.pmd = PMD_ALWAYS; f.sat = 0;
        f.dst = dst; f.dst_class = dst_class; f.wmsk = wmsk;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        f.s0_neg = s0_neg ? 1 : 0; f.s0_abs = 0;
        f.s1c = s1c; f.s1idx = s1idx; f.sw1 = sw1;
        f.s1_neg = s1_neg ? 1 : 0; f.s1_abs = 0;
        f.s2idx = s2idx; f.s2_neg = 0; f.s2_abs = 0;
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
                   uint8_t s1c, uint8_t s1idx, uint8_t sw1) {
        AluFields f{};
        f.op = setp_op; f.pmd = PMD_ALWAYS;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        f.s1c = s1c; f.s1idx = s1idx; f.sw1 = sw1;
        code.push_back(encode_alu(f));
    }

    void emit_flow(uint8_t op) {
        FlowFields f{};
        f.op = op; f.pmd = PMD_ALWAYS;
        code.push_back(encode_flow(f));
    }

    static uint8_t make_swizzle(const std::string& s) {
        if (s.empty() || s.size() > 4) return SWIZZLE_IDENTITY;
        auto idx = [](char c) -> int {
            switch (c) { case 'x': return 0; case 'y': return 1; case 'z': return 2; case 'w': return 3; }
            return -1;
        };
        uint8_t comp[4] = {0,0,0,0};
        for (size_t i = 0; i < s.size(); ++i) {
            int v = idx(s[i]); if (v < 0) return SWIZZLE_IDENTITY;
            comp[i] = static_cast<uint8_t>(v);
        }
        for (size_t i = s.size(); i < 4; ++i) comp[i] = comp[s.size() - 1];
        return static_cast<uint8_t>(comp[0] | (comp[1] << 2) | (comp[2] << 4) | (comp[3] << 6));
    }
    static uint8_t make_wmask(const std::string& s) {
        if (s.empty()) return 0xF;
        uint8_t m = 0;
        for (char c : s) {
            switch (c) {
                case 'x': m |= 1; break;
                case 'y': m |= 2; break;
                case 'z': m |= 4; break;
                case 'w': m |= 8; break;
                default: return 0xF;
            }
        }
        return m;
    }

    bool bind(const Decl& d) {
        VarBinding vb;
        vb.type = d.type;
        if (d.qualifier == "attribute") {
            vb.kind = VarBinding::ATTRIBUTE;
            vb.slot = next_attr++;
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
            if (stage == ShaderStage::Vertex) {
                vb.kind = VarBinding::VARYING_OUT;
                vb.slot = next_vsout++;
                out.varyings_out.push_back({d.name, d.type, vb.slot});
            } else {
                vb.kind = VarBinding::VARYING_IN;
                vb.slot = next_fsin++;
                out.varyings_in.push_back({d.name, d.type, vb.slot});
            }
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
                vars[name] = {VarBinding::BUILTIN_OUT, "vec4", 0};
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
        Operand o;
        switch (vb->kind) {
            case VarBinding::ATTRIBUTE:
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::UNIFORM_VEC:
                o.cls = SRC_CONST; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::VARYING_IN:
                o.cls = SRC_VARYING; o.idx = static_cast<uint8_t>(vb->slot); break;
            case VarBinding::LOCAL:
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot); break;
            default: return std::nullopt;
        }
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

    std::optional<Operand> emit_call(const Call& c) {
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
        VarBinding* lhs = lookup(name);
        if (!lhs) return false;
        uint8_t wmsk = make_wmask(member);
        if (lhs->kind == VarBinding::BUILTIN_OUT) {
            emit_alu(0x01, 1, encode_dst_out(0), wmsk,
                     rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
            return true;
        }
        if (lhs->kind == VarBinding::VARYING_OUT) {
            emit_alu(0x01, 1, encode_dst_out(static_cast<uint8_t>(lhs->slot)), wmsk,
                     rhs->cls, rhs->idx, rhs->swizzle, 0, 0, SWIZZLE_IDENTITY, 0,
                     rhs->neg, false);
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
        uint8_t setp_op = 0;
        switch (ifs.cond_op) {
            case Tok::EQ: setp_op = 0x18; break;
            case Tok::NE: setp_op = 0x19; break;
            case Tok::LT: setp_op = 0x1A; break;
            case Tok::LE: setp_op = 0x1B; break;
            case Tok::GT: setp_op = 0x1C; break;
            case Tok::GE: setp_op = 0x1D; break;
            default: return false;
        }
        emit_setp(setp_op, a->cls, a->idx, a->swizzle,
                  b->cls, b->idx, b->swizzle);
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
        return false;
    }
};

}  // namespace

CompileResult compile(const std::string& source, ShaderStage stage) {
    CompileResult result;
    Parser p(source);
    Program prog;
    if (!p.parse(prog)) {
        result.error = p.error;
        result.error_line = p.error_line;
        return result;
    }

    Codegen cg(stage, result);
    for (const auto& d : prog.globals) cg.bind(d);

    for (const auto& s : prog.body) {
        if (!cg.emit_stmt(s)) {
            result.error = "codegen failed at statement";
            result.error_line = s.line;
            return result;
        }
    }
    return result;
}

}  // namespace gpu::glsl
