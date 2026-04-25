// Sprint-1 assembler for GPU shader ISA v1.0.
//
// Hand-written recursive-descent style parser. Designed to handle the syntax
// used by tests/shader_corpus/ref_shader_*. Not a full GLSL/SPIR-V driver —
// that's a Phase 1 follow-up.
//
// Syntax recap (see docs/isa_spec.md §8):
//   [(p|!p)] op[.sat] dst[.wmask], src0[.swiz][modifier], src1, [src2]
// Source modifier: -src or |src| (or both: -|src|)

#include "gpu_compiler/asm.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "gpu_compiler/encoding.h"

namespace gpu::asm_ {
namespace {

using namespace gpu::isa;

struct OpInfo {
    uint8_t op;
    Format  fmt;
    uint8_t operands;        // 0 (nullary), 1, 2, 3
    bool    is_setp;
    bool    is_kil_or_break; // flow ops that don't take operands
};

const std::unordered_map<std::string, OpInfo> kOpcodes = {
    // ALU
    {"nop",     {0x00, Format::ALU,  0, false, false}},
    {"mov",     {0x01, Format::ALU,  1, false, false}},
    {"add",     {0x02, Format::ALU,  2, false, false}},
    {"mul",     {0x03, Format::ALU,  2, false, false}},
    {"dp2",     {0x04, Format::ALU,  2, false, false}},
    {"dp3",     {0x05, Format::ALU,  2, false, false}},
    {"dp4",     {0x06, Format::ALU,  2, false, false}},
    {"rcp",     {0x07, Format::ALU,  1, false, false}},
    {"rsq",     {0x08, Format::ALU,  1, false, false}},
    {"exp",     {0x09, Format::ALU,  1, false, false}},
    {"log",     {0x0A, Format::ALU,  1, false, false}},
    {"sin",     {0x0B, Format::ALU,  1, false, false}},
    {"cos",     {0x0C, Format::ALU,  1, false, false}},
    {"min",     {0x0D, Format::ALU,  2, false, false}},
    {"max",     {0x0E, Format::ALU,  2, false, false}},
    {"abs",     {0x0F, Format::ALU,  1, false, false}},
    {"frc",     {0x10, Format::ALU,  1, false, false}},
    {"flr",     {0x11, Format::ALU,  1, false, false}},
    {"mad",     {0x12, Format::ALU,  3, false, false}},
    {"cmp",     {0x13, Format::ALU,  3, false, false}},
    // Predicate set
    {"setp_eq", {0x18, Format::ALU,  2, true,  false}},
    {"setp_ne", {0x19, Format::ALU,  2, true,  false}},
    {"setp_lt", {0x1A, Format::ALU,  2, true,  false}},
    {"setp_le", {0x1B, Format::ALU,  2, true,  false}},
    {"setp_gt", {0x1C, Format::ALU,  2, true,  false}},
    {"setp_ge", {0x1D, Format::ALU,  2, true,  false}},
    // Flow
    {"bra",     {0x20, Format::FLOW, 0, false, false}},  // takes label, special
    {"call",    {0x21, Format::FLOW, 0, false, false}},
    {"ret",     {0x22, Format::FLOW, 0, false, false}},
    {"loop",    {0x23, Format::FLOW, 0, false, false}},  // takes immediate
    {"endloop", {0x24, Format::FLOW, 0, false, false}},
    {"break",   {0x25, Format::FLOW, 0, false, true}},
    {"if_p",    {0x26, Format::FLOW, 0, false, false}},
    {"else",    {0x27, Format::FLOW, 0, false, false}},
    {"endif",   {0x28, Format::FLOW, 0, false, false}},
    {"kil",     {0x29, Format::FLOW, 0, false, true}},
    // Memory
    {"ld",      {0x30, Format::MEM,  2, false, false}},
    {"st",      {0x31, Format::MEM,  2, false, false}},
    // Texture
    {"tex",     {0x34, Format::MEM,  2, false, false}},
    {"texb",    {0x35, Format::MEM,  2, false, false}},
    {"texl",    {0x36, Format::MEM,  2, false, false}},
    {"texg",    {0x37, Format::MEM,  3, false, false}},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string strip_inline_comment(const std::string& s) {
    auto p = s.find(';');
    if (p == std::string::npos) return s;
    return s.substr(0, p);
}

std::string trim(std::string s) {
    auto issp = [](unsigned char c) { return std::isspace(c); };
    while (!s.empty() && issp(s.front())) s.erase(s.begin());
    while (!s.empty() && issp(s.back()))  s.pop_back();
    return s;
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') { out.push_back(trim(cur)); cur.clear(); }
        else          { cur += c; }
    }
    if (!cur.empty()) out.push_back(trim(cur));
    return out;
}

// Map swizzle string ("x", "xy", "xyzw", "xxxx", "yyyy", ...) to 8-bit.
// Missing components fill with last specified.
std::optional<uint8_t> parse_swizzle(const std::string& s) {
    if (s.empty() || s.size() > 4) return std::nullopt;
    uint8_t comp[4] = {0, 0, 0, 0};
    auto idx = [](char c) -> int {
        switch (c) {
            case 'x': case 'X': return 0;
            case 'y': case 'Y': return 1;
            case 'z': case 'Z': return 2;
            case 'w': case 'W': return 3;
            default: return -1;
        }
    };
    for (size_t i = 0; i < s.size(); ++i) {
        int v = idx(s[i]);
        if (v < 0) return std::nullopt;
        comp[i] = static_cast<uint8_t>(v);
    }
    // Pad with last (if any) so undef components have a defined source.
    for (size_t i = s.size(); i < 4; ++i) comp[i] = comp[s.size() - 1];
    return static_cast<uint8_t>(comp[0] | (comp[1] << 2) | (comp[2] << 4) | (comp[3] << 6));
}

// Map write-mask string ("x", "xy", "xyz", "xyzw") to 4-bit.
std::optional<uint8_t> parse_wmask(const std::string& s) {
    if (s.empty() || s.size() > 4) return std::nullopt;
    uint8_t m = 0;
    for (char c : s) {
        switch (c) {
            case 'x': case 'X': m |= 0x1; break;
            case 'y': case 'Y': m |= 0x2; break;
            case 'z': case 'Z': m |= 0x4; break;
            case 'w': case 'W': m |= 0x8; break;
            default: return std::nullopt;
        }
    }
    return m;
}

struct Operand {
    enum Class { GPR, CONST, VARYING, OUTPUT, PREDICATE, TEX } cls;
    uint8_t  index = 0;
    uint8_t  swizzle = SWIZZLE_IDENTITY;   // 8-bit (for sources)
    uint8_t  wmask = 0xF;                  // 4-bit (for destinations)
    bool     neg = false;
    bool     abs = false;
};

// Parse operand text like "r3.xyz", "-c4.xxx", "|v0|.x", etc.
// `is_dst` distinguishes write-mask vs swizzle interpretation of the suffix.
std::optional<Operand> parse_operand(std::string s, bool is_dst) {
    Operand o;
    s = trim(s);
    if (s.empty()) return std::nullopt;

    // Modifiers
    if (s.front() == '-') { o.neg = true; s.erase(s.begin()); s = trim(s); }
    if (s.size() >= 2 && s.front() == '|' && s.back() == '|') {
        o.abs = true;
        s = s.substr(1, s.size() - 2);
        s = trim(s);
    }

    // Suffix split (text after first '.')
    std::string body = s;
    std::string suffix;
    auto dotp = s.find('.');
    if (dotp != std::string::npos) {
        body = s.substr(0, dotp);
        suffix = s.substr(dotp + 1);
    }
    body = trim(body);
    suffix = trim(suffix);

    // Class + index
    if (body == "p") {
        o.cls = Operand::PREDICATE;
    } else if (!body.empty() && (body[0] == 'r' || body[0] == 'R')) {
        o.cls = Operand::GPR;
        o.index = static_cast<uint8_t>(std::stoi(body.substr(1)));
    } else if (!body.empty() && (body[0] == 'c' || body[0] == 'C')) {
        o.cls = Operand::CONST;
        o.index = static_cast<uint8_t>(std::stoi(body.substr(1)));
    } else if (!body.empty() && (body[0] == 'v' || body[0] == 'V')) {
        o.cls = Operand::VARYING;
        o.index = static_cast<uint8_t>(std::stoi(body.substr(1)));
    } else if (!body.empty() && (body[0] == 'o' || body[0] == 'O')) {
        o.cls = Operand::OUTPUT;
        o.index = static_cast<uint8_t>(std::stoi(body.substr(1)));
    } else if (body.rfind("tex", 0) == 0) {
        o.cls = Operand::TEX;
        o.index = static_cast<uint8_t>(std::stoi(body.substr(3)));
    } else {
        return std::nullopt;
    }

    // Apply suffix
    if (!suffix.empty()) {
        if (is_dst && o.cls != Operand::PREDICATE) {
            auto m = parse_wmask(suffix);
            if (!m) return std::nullopt;
            o.wmask = *m;
        } else {
            auto sw = parse_swizzle(suffix);
            if (!sw) return std::nullopt;
            o.swizzle = *sw;
        }
    } else if (is_dst) {
        o.wmask = 0xF;
    }

    return o;
}

uint8_t src_class_of(const Operand& o) {
    switch (o.cls) {
        case Operand::GPR:     return SRC_GPR;
        case Operand::CONST:   return SRC_CONST;
        case Operand::VARYING: return SRC_VARYING;
        default:               return SRC_GPR;     // not reached
    }
}

// ---------------------------------------------------------------------------
// Per-line assembly
// ---------------------------------------------------------------------------

bool assemble_line(const std::string& raw_line, AssembleResult& out, int lineno) {
    std::string line = trim(strip_inline_comment(raw_line));
    if (line.empty()) return true;

    // Predicate prefix
    uint8_t pmd = PMD_ALWAYS;
    if (line.front() == '(') {
        auto rp = line.find(')');
        if (rp == std::string::npos) {
            out.error = "missing ')'";
            out.error_line = lineno;
            return false;
        }
        std::string p = trim(line.substr(1, rp - 1));
        if (p == "p")        pmd = PMD_IF_P;
        else if (p == "!p")  pmd = PMD_IF_NOT_P;
        else {
            out.error = "unknown predicate '" + p + "'";
            out.error_line = lineno;
            return false;
        }
        line = trim(line.substr(rp + 1));
    }

    // Mnemonic + optional .sat
    auto sp = line.find_first_of(" \t");
    std::string mnem = (sp == std::string::npos) ? line : line.substr(0, sp);
    std::string rest = (sp == std::string::npos) ? "" : trim(line.substr(sp));
    bool sat = false;
    auto dot = mnem.find('.');
    if (dot != std::string::npos) {
        std::string tail = mnem.substr(dot + 1);
        if (tail == "sat") { sat = true; mnem.resize(dot); }
        else { out.error = "unknown mnemonic suffix ." + tail; out.error_line = lineno; return false; }
    }

    auto it = kOpcodes.find(mnem);
    if (it == kOpcodes.end()) {
        out.error = "unknown opcode '" + mnem + "'";
        out.error_line = lineno;
        return false;
    }
    const OpInfo& info = it->second;

    // Special flow forms with no operands
    if (info.fmt == Format::FLOW && info.is_kil_or_break) {
        FlowFields f{};
        f.op  = info.op;
        f.pmd = pmd;
        out.code.push_back(encode_flow(f));
        return true;
    }
    if (info.fmt == Format::FLOW && (mnem == "ret" || mnem == "endloop" ||
                                      mnem == "if_p" || mnem == "else" || mnem == "endif")) {
        FlowFields f{};
        f.op  = info.op;
        f.pmd = pmd;
        out.code.push_back(encode_flow(f));
        return true;
    }
    if (info.fmt == Format::FLOW && mnem == "loop") {
        // loop <count>
        FlowFields f{};
        f.op = info.op;
        f.pmd = pmd;
        try { f.imm = std::stoll(rest); }
        catch (...) { out.error = "loop expects integer count"; out.error_line = lineno; return false; }
        out.code.push_back(encode_flow(f));
        return true;
    }
    // bra / call: label not supported in Sprint 1 — would need a 2-pass.
    if (info.fmt == Format::FLOW && (mnem == "bra" || mnem == "call")) {
        out.error = mnem + ": label-based branching not supported yet (Sprint 1)";
        out.error_line = lineno;
        return false;
    }

    // Parse operands
    auto opnds = split_csv(rest);

    if (info.fmt == Format::ALU) {
        if (info.is_setp) {
            // setp_<cond> p, src0, src1
            if (opnds.size() != 3) { out.error = "setp_* expects 3 operands"; out.error_line = lineno; return false; }
            auto pdst = parse_operand(opnds[0], /*is_dst=*/true);
            auto s0   = parse_operand(opnds[1], false);
            auto s1   = parse_operand(opnds[2], false);
            if (!pdst || pdst->cls != Operand::PREDICATE) { out.error = "setp_* dst must be 'p'"; out.error_line = lineno; return false; }
            if (!s0 || !s1) { out.error = "setp_* operand parse error"; out.error_line = lineno; return false; }
            AluFields f{};
            f.op  = info.op;
            f.pmd = pmd;
            // For setp, dst encoding is unused (predicate hard-coded);
            // we leave dst=0/dst_class=0/wmsk=0.
            f.s0c = src_class_of(*s0); f.s0idx = s0->index; f.sw0 = s0->swizzle;
            f.s0_neg = s0->neg; f.s0_abs = s0->abs;
            f.s1c = src_class_of(*s1); f.s1idx = s1->index; f.sw1 = s1->swizzle;
            f.s1_neg = s1->neg; f.s1_abs = s1->abs;
            out.code.push_back(encode_alu(f));
            return true;
        }

        if (info.operands == 0) {
            AluFields f{};
            f.op = info.op; f.pmd = pmd; f.sat = sat ? 1 : 0;
            out.code.push_back(encode_alu(f));
            return true;
        }

        if (opnds.size() < static_cast<size_t>(1 + info.operands)) {
            out.error = "too few operands for " + mnem;
            out.error_line = lineno;
            return false;
        }

        auto dst = parse_operand(opnds[0], /*is_dst=*/true);
        if (!dst) { out.error = "dst parse error"; out.error_line = lineno; return false; }

        AluFields f{};
        f.op   = info.op;
        f.pmd  = pmd;
        f.sat  = sat ? 1 : 0;
        if (dst->cls == Operand::OUTPUT) {
            f.dst_class = 1;
            f.dst       = encode_dst_out(dst->index);
        } else if (dst->cls == Operand::GPR) {
            f.dst_class = 0;
            f.dst       = encode_dst_gpr(dst->index);
        } else {
            out.error = "dst must be GPR or output";
            out.error_line = lineno;
            return false;
        }
        f.wmsk = dst->wmask;

        auto src0 = parse_operand(opnds[1], false);
        if (!src0) { out.error = "src0 parse error"; out.error_line = lineno; return false; }
        f.s0c = src_class_of(*src0); f.s0idx = src0->index; f.sw0 = src0->swizzle;
        f.s0_neg = src0->neg; f.s0_abs = src0->abs;

        if (info.operands >= 2) {
            auto src1 = parse_operand(opnds[2], false);
            if (!src1) { out.error = "src1 parse error"; out.error_line = lineno; return false; }
            f.s1c = src_class_of(*src1); f.s1idx = src1->index; f.sw1 = src1->swizzle;
            f.s1_neg = src1->neg; f.s1_abs = src1->abs;
        }
        if (info.operands == 3) {
            auto src2 = parse_operand(opnds[3], false);
            if (!src2 || src2->cls != Operand::GPR) {
                out.error = "src2 must be GPR";
                out.error_line = lineno; return false;
            }
            f.s2idx = src2->index;
            f.s2_neg = src2->neg; f.s2_abs = src2->abs;
        }
        out.code.push_back(encode_alu(f));
        return true;
    }

    if (info.fmt == Format::MEM) {
        // tex / ld / st: dst, src(.swiz), [tex_slot | imm]
        if (opnds.size() < 2) { out.error = "memory op needs >= 2 operands"; out.error_line = lineno; return false; }
        auto dst = parse_operand(opnds[0], /*is_dst=*/true);
        auto src = parse_operand(opnds[1], false);
        if (!dst || !src) { out.error = "operand parse error"; out.error_line = lineno; return false; }
        MemFields f{};
        f.op  = info.op;
        f.pmd = pmd;
        f.dst = (dst->cls == Operand::OUTPUT) ? encode_dst_out(dst->index) : encode_dst_gpr(dst->index);
        f.wmsk = dst->wmask;
        f.src = src->index;
        f.src_swiz = src->swizzle;
        if (mnem == "tex" || mnem == "texb" || mnem == "texl" || mnem == "texg") {
            if (opnds.size() < 3) { out.error = "texture op needs binding"; out.error_line = lineno; return false; }
            auto t = parse_operand(opnds[2], false);
            if (!t || t->cls != Operand::TEX) { out.error = "expected texN binding"; out.error_line = lineno; return false; }
            f.tex = t->index;
            f.mode = (mnem == "tex") ? 0 : (mnem == "texb") ? 1 : (mnem == "texl") ? 2 : 3;
        } else {
            // ld / st: optional immediate (not used in ref shaders yet)
            f.mode = (mnem == "ld") ? 0 : 1;
            if (opnds.size() >= 3) {
                try { f.imm = std::stoi(opnds[2]); }
                catch (...) { out.error = "expected integer offset"; out.error_line = lineno; return false; }
            }
        }
        out.code.push_back(encode_mem(f));
        return true;
    }

    out.error = "unhandled opcode (Sprint 1)";
    out.error_line = lineno;
    return false;
}

}  // namespace

AssembleResult assemble(const std::string& source) {
    AssembleResult result;
    std::istringstream is(source);
    std::string line;
    int lineno = 0;
    while (std::getline(is, line)) {
        ++lineno;
        if (!assemble_line(line, result, lineno)) return result;
    }
    return result;
}

}  // namespace gpu::asm_
