// Disassembler: binary -> canonical assembly text.
// Output is intentionally regular (always full 4-char swizzle, full mask
// only when not all xyzw set) so that round-tripping is well-defined.

#include "gpu_compiler/asm.h"
#include "gpu_compiler/encoding.h"

#include <sstream>
#include <unordered_map>

namespace gpu::asm_ {
namespace {

using namespace gpu::isa;

const std::unordered_map<uint8_t, std::string> kAluNames = {
    {0x00, "nop"}, {0x01, "mov"}, {0x02, "add"}, {0x03, "mul"},
    {0x04, "dp2"}, {0x05, "dp3"}, {0x06, "dp4"},
    {0x07, "rcp"}, {0x08, "rsq"}, {0x09, "exp"}, {0x0A, "log"},
    {0x0B, "sin"}, {0x0C, "cos"},
    {0x0D, "min"}, {0x0E, "max"}, {0x0F, "abs"},
    {0x10, "frc"}, {0x11, "flr"},
    {0x12, "mad"}, {0x13, "cmp"},
    {0x18, "setp_eq"}, {0x19, "setp_ne"},
    {0x1A, "setp_lt"}, {0x1B, "setp_le"},
    {0x1C, "setp_gt"}, {0x1D, "setp_ge"},
};
const std::unordered_map<uint8_t, std::string> kFlowNames = {
    {0x20, "bra"}, {0x21, "call"}, {0x22, "ret"},
    {0x23, "loop"}, {0x24, "endloop"}, {0x25, "break"},
    {0x26, "if_p"}, {0x27, "else"}, {0x28, "endif"},
    {0x29, "kil"},
};
const std::unordered_map<uint8_t, std::string> kMemNames = {
    {0x30, "ld"}, {0x31, "st"},
    {0x34, "tex"}, {0x35, "texb"}, {0x36, "texl"}, {0x37, "texg"},
};

bool is_setp_op(uint8_t op) { return op >= 0x18 && op <= 0x1D; }
int  alu_operands(uint8_t op) {
    if (op == 0x00) return 0;
    if (op == 0x01 || op == 0x07 || op == 0x08 || op == 0x09 || op == 0x0A ||
        op == 0x0B || op == 0x0C || op == 0x0F || op == 0x10 || op == 0x11) return 1;
    if (op == 0x12 || op == 0x13) return 3;
    return 2;
}

std::string swizzle_text(uint8_t sw) {
    char out[5] = {0,0,0,0,0};
    for (int i = 0; i < 4; ++i) out[i] = "xyzw"[component(sw, i)];
    return out;
}

std::string mask_text(uint8_t m) {
    if (m == 0xF) return "";
    std::string s = ".";
    if (wmsk_x(m)) s += "x";
    if (wmsk_y(m)) s += "y";
    if (wmsk_z(m)) s += "z";
    if (wmsk_w(m)) s += "w";
    return s;
}

std::string src_text(uint8_t cls, uint8_t idx, uint8_t sw, bool neg, bool abs_) {
    const char* prefix = (cls == SRC_GPR) ? "r"
                       : (cls == SRC_CONST) ? "c"
                       : (cls == SRC_VARYING) ? "v" : "?";
    std::ostringstream o;
    if (neg) o << "-";
    if (abs_) o << "|";
    o << prefix << static_cast<int>(idx) << "." << swizzle_text(sw);
    if (abs_) o << "|";
    return o.str();
}

std::string dst_text(uint8_t dst, uint8_t dst_class, uint8_t mask) {
    std::ostringstream o;
    if (dst_class) o << "o" << static_cast<int>(dst & 0x3);
    else            o << "r" << static_cast<int>(dst & 0x1F);
    o << mask_text(mask);
    return o.str();
}

std::string pmd_prefix(uint8_t pmd) {
    if (pmd == PMD_IF_P)     return "(p) ";
    if (pmd == PMD_IF_NOT_P) return "(!p) ";
    return "";
}

std::string disasm_alu(Inst i) {
    AluFields f = decode_alu(i);
    auto it = kAluNames.find(f.op);
    std::string name = (it == kAluNames.end()) ? "<?alu>" : it->second;
    std::ostringstream o;
    o << pmd_prefix(f.pmd);
    o << name;
    if (f.sat) o << ".sat";
    o << " ";

    if (is_setp_op(f.op)) {
        o << "p, "
          << src_text(f.s0c, f.s0idx, f.sw0, f.s0_neg, f.s0_abs) << ", "
          << src_text(f.s1c, f.s1idx, f.sw1, f.s1_neg, f.s1_abs);
        return o.str();
    }

    int n = alu_operands(f.op);
    if (n == 0) return o.str();

    o << dst_text(f.dst, f.dst_class, f.wmsk) << ", "
      << src_text(f.s0c, f.s0idx, f.sw0, f.s0_neg, f.s0_abs);
    if (n >= 2) o << ", " << src_text(f.s1c, f.s1idx, f.sw1, f.s1_neg, f.s1_abs);
    if (n == 3) o << ", " << src_text(SRC_GPR, f.s2idx, SWIZZLE_IDENTITY, f.s2_neg, f.s2_abs);
    return o.str();
}

std::string disasm_flow(Inst i) {
    FlowFields f = decode_flow(i);
    auto it = kFlowNames.find(f.op);
    std::string name = (it == kFlowNames.end()) ? "<?flow>" : it->second;
    std::ostringstream o;
    o << pmd_prefix(f.pmd) << name;
    if (name == "loop") o << " " << f.imm;
    return o.str();
}

std::string disasm_mem(Inst i) {
    MemFields f = decode_mem(i);
    auto it = kMemNames.find(f.op);
    std::string name = (it == kMemNames.end()) ? "<?mem>" : it->second;
    std::ostringstream o;
    o << pmd_prefix(f.pmd) << name << " ";

    // Reconstruct dst (now class-aware) / src (now class-aware).
    {
        std::ostringstream d;
        if (f.dst_class) d << "o" << static_cast<int>(f.dst & 0x3);
        else             d << "r" << static_cast<int>(f.dst & 0x1F);
        d << mask_text(f.wmsk);
        o << d.str();
    }
    o << ", ";
    {
        const char* prefix = (f.src_class == SRC_GPR) ? "r"
                           : (f.src_class == SRC_CONST) ? "c"
                           : (f.src_class == SRC_VARYING) ? "v" : "?";
        std::ostringstream s;
        s << prefix << static_cast<int>(f.src) << "." << swizzle_text(f.src_swiz);
        o << s.str();
    }
    if (name.rfind("tex", 0) == 0) {
        o << ", tex" << static_cast<int>(f.tex);
    } else if (f.imm) {
        o << ", " << f.imm;
    }
    return o.str();
}

}  // namespace

std::string disassemble(const std::vector<isa::Inst>& code) {
    std::ostringstream o;
    for (auto inst : code) {
        uint8_t op = bits(inst, 63, 58);
        Format fmt = format_of(op);
        switch (fmt) {
            case Format::ALU:  o << disasm_alu(inst);  break;
            case Format::FLOW: o << disasm_flow(inst); break;
            case Format::MEM:  o << disasm_mem(inst);  break;
            default: o << "<?op " << static_cast<int>(op) << ">"; break;
        }
        o << "\n";
    }
    return o.str();
}

}  // namespace gpu::asm_
