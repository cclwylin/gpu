// Minimal GLSL ES 2.0 subset → ISA compiler.
//
// Single-pass naive codegen: each AST node emits directly into an instruction
// stream, with a tiny register allocator that maps named variables to GPRs.
//
// This is intentionally simple. The architecture (lexer / parser / AST / cg)
// is the interesting part; the optimisations (SSA, swizzle squashing,
// constant folding) belong in a Sprint 2.x follow-up.

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
    KW_VOID, KW_MAIN, KW_ATTRIBUTE, KW_UNIFORM, KW_VARYING, KW_PRECISION,
    KW_HIGHP, KW_MEDIUMP, KW_LOWP,
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
            // line comment
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
            return {Tok::ID, s, line};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
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
struct MemberAccess { ExprPtr base; std::string member; };
struct BinaryOp { char op; ExprPtr lhs, rhs; };
struct UnaryOp  { char op; ExprPtr expr; };
struct Call     { std::string name; std::vector<ExprPtr> args; };

struct Expr : std::variant<Identifier, MemberAccess, BinaryOp, UnaryOp, Call> {
    using variant::variant;
    int line = 0;
};

struct Decl {
    std::string qualifier;   // "attribute" | "uniform" | "varying" | "" (local)
    std::string type;        // "vec4" / "mat4" / ...
    std::string name;
    int line = 0;
};

struct Stmt {
    ExprPtr lhs;
    ExprPtr rhs;
    int line = 0;
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
            // precision qualifiers (eat and ignore)
            if (cur.kind == Tok::KW_PRECISION) {
                cur = lex.next();
                if (cur.kind == Tok::KW_HIGHP || cur.kind == Tok::KW_MEDIUMP ||
                    cur.kind == Tok::KW_LOWP) cur = lex.next();
                if (cur.kind == Tok::ID) cur = lex.next();   // type name
                if (!expect(Tok::SEMI, "expected ';' after precision")) return false;
                continue;
            }
            std::string qualifier;
            if (cur.kind == Tok::KW_ATTRIBUTE) { qualifier = "attribute"; cur = lex.next(); }
            else if (cur.kind == Tok::KW_UNIFORM) { qualifier = "uniform"; cur = lex.next(); }
            else if (cur.kind == Tok::KW_VARYING) { qualifier = "varying"; cur = lex.next(); }

            // optional precision after qualifier
            if (cur.kind == Tok::KW_HIGHP || cur.kind == Tok::KW_MEDIUMP ||
                cur.kind == Tok::KW_LOWP) cur = lex.next();

            if (qualifier.empty() && cur.kind == Tok::KW_VOID) {
                cur = lex.next();
                if (!expect(Tok::KW_MAIN, "expected 'main'")) return false;
                if (!expect(Tok::LPAREN, "expected '('")) return false;
                if (!expect(Tok::RPAREN, "expected ')'")) return false;
                if (!parse_block(p)) return false;
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

    bool parse_block(Program& p) {
        if (!expect(Tok::LBRACE, "expected '{'")) return false;
        while (cur.kind != Tok::RBRACE && cur.kind != Tok::EOF_) {
            Stmt s;
            s.line = cur.line;
            // assignment: lhs = expr ;
            auto lhs = parse_unary();
            if (!lhs) return false;
            if (!expect(Tok::ASSIGN, "expected '='")) return false;
            auto rhs = parse_expr();
            if (!rhs) return false;
            if (!expect(Tok::SEMI, "expected ';'")) return false;
            s.lhs = lhs;
            s.rhs = rhs;
            p.body.push_back(std::move(s));
        }
        if (!expect(Tok::RBRACE, "expected '}'")) return false;
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
                error_line = cur.line;
                return nullptr;
            }
            auto m = std::make_shared<Expr>(MemberAccess{base, cur.lex});
            m->line = cur.line;
            base = m;
            cur = lex.next();
        }
        return base;
    }
    ExprPtr parse_primary() {
        if (cur.kind == Tok::ID) {
            std::string n = cur.lex;
            cur = lex.next();
            if (cur.kind == Tok::LPAREN) {   // function call (could be ctor too)
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
        error_line = cur.line;
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// Codegen
// -----------------------------------------------------------------------------
struct VarBinding {
    enum Kind { ATTRIBUTE, UNIFORM_VEC, UNIFORM_MAT4, VARYING_OUT, VARYING_IN, BUILTIN_OUT, SAMPLER, LOCAL };
    Kind        kind;
    std::string type;
    int         slot = 0;     // attribute slot, uniform c-base, varying o/v slot, sampler binding
};

struct Codegen {
    ShaderStage stage;
    CompileResult& out;
    std::vector<Inst>& code;

    std::unordered_map<std::string, VarBinding> vars;
    int next_attr = 0;
    int next_const = 0;     // packed vec4 slot in c-bank
    int next_vsout = 1;     // o1..o7 (o0 is gl_Position / gl_FragColor)
    int next_fsin = 0;      // v0..v7
    int next_sampler = 0;
    int next_gpr = 0;       // 0..15 free for temps

    explicit Codegen(ShaderStage st, CompileResult& o) : stage(st), out(o), code(o.code) {}

    int alloc_gpr() { return next_gpr++; }

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

    void emit_mem(uint8_t op, uint8_t dst, uint8_t wmsk,
                  uint8_t src, uint8_t src_swiz, uint8_t tex, uint8_t mode = 0) {
        MemFields m{};
        m.op = op; m.pmd = PMD_ALWAYS;
        m.dst = dst; m.wmsk = wmsk;
        m.src = src; m.src_swiz = src_swiz;
        m.tex = tex; m.mode = mode;
        code.push_back(encode_mem(m));
    }

    // Convert a member-access string like "xy" into an 8-bit swizzle, with
    // last component repeated to fill 4 lanes.
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

    // -------------------------------------------------------------------
    // Allocate global vars to ABI slots.
    // -------------------------------------------------------------------
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

    // Built-in identifiers are bound on first reference.
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

    // -------------------------------------------------------------------
    // Operand descriptor used by emitter: where data lives.
    // -------------------------------------------------------------------
    struct Operand {
        uint8_t cls = 0;        // SrcClass for source side
        uint8_t idx = 0;
        uint8_t swizzle = SWIZZLE_IDENTITY;
        bool    is_const_block = false;
        bool    is_dst_output  = false;
        bool    is_dst_gpr     = false;
        bool    neg = false;
    };

    // Materialise an expression into a GPR; return its operand descriptor as a source.
    std::optional<Operand> emit_expr(const ExprPtr& e) {
        if (auto id = std::get_if<Identifier>(e.get())) return emit_identifier(id->name);
        if (auto m  = std::get_if<MemberAccess>(e.get())) return emit_member(*m);
        if (auto bo = std::get_if<BinaryOp>(e.get())) return emit_binop(*bo);
        if (auto u  = std::get_if<UnaryOp>(e.get())) return emit_unop(*u);
        if (auto c  = std::get_if<Call>(e.get())) return emit_call(*c);
        return std::nullopt;
    }

    // --- identifier ---
    std::optional<Operand> emit_identifier(const std::string& name) {
        VarBinding* vb = lookup(name);
        if (!vb) return std::nullopt;
        Operand o;
        switch (vb->kind) {
            case VarBinding::ATTRIBUTE: {
                // Attributes pre-loaded into r0..r{N-1} per host convention.
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot);
                break;
            }
            case VarBinding::UNIFORM_VEC: {
                o.cls = SRC_CONST; o.idx = static_cast<uint8_t>(vb->slot);
                break;
            }
            case VarBinding::VARYING_IN: {
                o.cls = SRC_VARYING; o.idx = static_cast<uint8_t>(vb->slot);
                break;
            }
            case VarBinding::LOCAL: {
                o.cls = SRC_GPR; o.idx = static_cast<uint8_t>(vb->slot);
                break;
            }
            default: return std::nullopt;     // mat4/sampler2D need member or call
        }
        return o;
    }

    // --- member access: take swizzle suffix on top of base operand ---
    std::optional<Operand> emit_member(const MemberAccess& m) {
        auto base = emit_expr(m.base);
        if (!base) return std::nullopt;
        base->swizzle = make_swizzle(m.member);
        return base;
    }

    // --- unary: only negate, materialise then mark neg ---
    std::optional<Operand> emit_unop(const UnaryOp& u) {
        auto inner = emit_expr(u.expr);
        if (!inner) return std::nullopt;
        if (u.op == '-') inner->neg = true;
        return inner;
    }

    // --- binary: lower into ALU ---
    std::optional<Operand> emit_binop(const BinaryOp& bo) {
        // Special: mat4 * vec4
        if (bo.op == '*') {
            // Detect mat4 * vec4: lhs identifier referring to mat4
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
        emit_alu(op, /*dst_class=*/0, encode_dst_gpr(dst_gpr), 0xF,
                 a->cls, a->idx, a->swizzle,
                 b->cls, b->idx, b->swizzle, 0,
                 a->neg, b->neg);
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
        return r;
    }

    // mat4 * vec4 -> 4 dp4 into a dest GPR's xyzw.
    std::optional<Operand> emit_mat4_vec4(const VarBinding& mat, const ExprPtr& vec_expr) {
        auto v = emit_expr(vec_expr);
        if (!v) return std::nullopt;
        int dst_gpr = alloc_gpr();
        for (int row = 0; row < 4; ++row) {
            uint8_t mask = static_cast<uint8_t>(1 << row);
            emit_alu(/*dp4*/ 0x06, 0, encode_dst_gpr(dst_gpr), mask,
                     SRC_CONST, static_cast<uint8_t>(mat.slot + row), SWIZZLE_IDENTITY,
                     v->cls, v->idx, v->swizzle, 0,
                     /*s0_neg=*/false, v->neg);
        }
        Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
        return r;
    }

    // --- function call (texture2D, dot, normalize, max, min) ---
    std::optional<Operand> emit_call(const Call& c) {
        if (c.name == "texture2D") {
            if (c.args.size() != 2) return std::nullopt;
            // First arg: sampler identifier
            auto* id = std::get_if<Identifier>(c.args[0].get());
            if (!id) return std::nullopt;
            VarBinding* sm = lookup(id->name);
            if (!sm || sm->kind != VarBinding::SAMPLER) return std::nullopt;
            // Second arg: uv expression -> materialise into GPR
            auto uv = emit_expr(c.args[1]);
            if (!uv) return std::nullopt;
            // ensure GPR carrier (TMU op reads from GPR.src)
            int uv_gpr;
            if (uv->cls == SRC_GPR) uv_gpr = uv->idx;
            else {
                uv_gpr = alloc_gpr();
                emit_alu(/*mov*/ 0x01, 0, encode_dst_gpr(uv_gpr), 0xF,
                         uv->cls, uv->idx, uv->swizzle);
            }
            int dst_gpr = alloc_gpr();
            emit_mem(/*tex*/ 0x34, encode_dst_gpr(dst_gpr), 0xF,
                     static_cast<uint8_t>(uv_gpr), uv->swizzle,
                     static_cast<uint8_t>(sm->slot), 0);
            Operand r; r.cls = SRC_GPR; r.idx = static_cast<uint8_t>(dst_gpr);
            return r;
        }
        // Future: dot/normalize/max/min. Keep parser-compatible by failing
        // softly in this sprint.
        return std::nullopt;
    }

    // -------------------------------------------------------------------
    // Statement emitter: lhs = rhs;
    // -------------------------------------------------------------------
    bool emit_stmt(const Stmt& s) {
        // RHS into an operand
        auto rhs = emit_expr(s.rhs);
        if (!rhs) return false;

        // LHS analysis: identifier (built-in or named) optionally with member
        std::string name;
        std::string member;
        if (auto* id = std::get_if<Identifier>(s.lhs.get())) {
            name = id->name;
        } else if (auto* mem = std::get_if<MemberAccess>(s.lhs.get())) {
            auto* id = std::get_if<Identifier>(mem->base.get());
            if (!id) return false;
            name = id->name;
            member = mem->member;
        } else {
            return false;
        }
        VarBinding* lhs = lookup(name);
        if (!lhs) return false;

        uint8_t wmsk = make_wmask(member);

        if (lhs->kind == VarBinding::BUILTIN_OUT) {
            // gl_Position / gl_FragColor → o0
            emit_alu(/*mov*/ 0x01, /*dst_class=*/1, encode_dst_out(0), wmsk,
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
