// ---------------------------------------------------------------------
// AUTO-GENERATED FROM specs/isa.yaml — DO NOT EDIT.
// Regenerate with: python3 tools/isa_gen/isa_gen.py
// ---------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace gpu::isa {

enum class Opcode : uint32_t {
    OP_NOP = 0x00,  // alu: no op
    OP_MOV = 0x01,  // alu: dst = src0
    OP_ADD = 0x02,  // alu: dst = src0 + src1
    OP_MUL = 0x03,  // alu: dst = src0 * src1
    OP_MAD = 0x04,  // alu: dst = src0*src1 + src2
    OP_DP3 = 0x05,  // alu: dst.xyzw = dot3(src0, src1)
    OP_DP4 = 0x06,  // alu: dst.xyzw = dot4(src0, src1)
    OP_RCP = 0x07,  // alu: dst.xyzw = 1/src0.x
    OP_RSQ = 0x08,  // alu: dst.xyzw = 1/sqrt(src0.x)
    OP_EXP = 0x09,  // alu: dst.xyzw = 2^src0.x
    OP_LOG = 0x0A,  // alu: dst.xyzw = log2(src0.x)
    OP_SIN = 0x0B,  // alu: dst.xyzw = sin(src0.x)
    OP_COS = 0x0C,  // alu: dst.xyzw = cos(src0.x)
    OP_MIN = 0x0D,  // alu: dst = min(src0, src1)
    OP_MAX = 0x0E,  // alu: dst = max(src0, src1)
    OP_ABS = 0x0F,  // alu: dst = abs(src0)
    OP_FRC = 0x10,  // alu: dst = src0 - floor(src0)
    OP_FLR = 0x11,  // alu: dst = floor(src0)
    OP_CMP = 0x12,  // alu: dst = src0>=0 ? src1 : src2
    OP_BRA = 0x20,  // flow: 
    OP_CALL = 0x21,  // flow: 
    OP_RET = 0x22,  // flow: 
    OP_LOOP = 0x23,  // flow: 
    OP_ENDLOOP = 0x24,  // flow: 
    OP_BREAK = 0x25,  // flow: 
    OP_KIL = 0x26,  // flow: discard pixel (FS only)
    OP_LD = 0x30,  // mem: dst = mem[src+imm]
    OP_ST = 0x31,  // mem: mem[src+imm] = dst
    OP_TEX = 0x34,  // mem: 
    OP_TEXB = 0x35,  // mem: 
    OP_TEXL = 0x36,  // mem: 
    OP_TEXG = 0x37,  // mem: 
};

inline const char* opcode_name(Opcode op) {
    switch (op) {
        case Opcode::OP_NOP: return "nop";
        case Opcode::OP_MOV: return "mov";
        case Opcode::OP_ADD: return "add";
        case Opcode::OP_MUL: return "mul";
        case Opcode::OP_MAD: return "mad";
        case Opcode::OP_DP3: return "dp3";
        case Opcode::OP_DP4: return "dp4";
        case Opcode::OP_RCP: return "rcp";
        case Opcode::OP_RSQ: return "rsq";
        case Opcode::OP_EXP: return "exp";
        case Opcode::OP_LOG: return "log";
        case Opcode::OP_SIN: return "sin";
        case Opcode::OP_COS: return "cos";
        case Opcode::OP_MIN: return "min";
        case Opcode::OP_MAX: return "max";
        case Opcode::OP_ABS: return "abs";
        case Opcode::OP_FRC: return "frc";
        case Opcode::OP_FLR: return "flr";
        case Opcode::OP_CMP: return "cmp";
        case Opcode::OP_BRA: return "bra";
        case Opcode::OP_CALL: return "call";
        case Opcode::OP_RET: return "ret";
        case Opcode::OP_LOOP: return "loop";
        case Opcode::OP_ENDLOOP: return "endloop";
        case Opcode::OP_BREAK: return "break";
        case Opcode::OP_KIL: return "kil";
        case Opcode::OP_LD: return "ld";
        case Opcode::OP_ST: return "st";
        case Opcode::OP_TEX: return "tex";
        case Opcode::OP_TEXB: return "texb";
        case Opcode::OP_TEXL: return "texl";
        case Opcode::OP_TEXG: return "texg";
        default: return "<unknown>";
    }
}

enum class Format { ALU, FLOW, MEM, UNKNOWN };
inline Format opcode_format(Opcode op) {
    switch (op) {
        case Opcode::OP_NOP: return Format::ALU;
        case Opcode::OP_MOV: return Format::ALU;
        case Opcode::OP_ADD: return Format::ALU;
        case Opcode::OP_MUL: return Format::ALU;
        case Opcode::OP_MAD: return Format::ALU;
        case Opcode::OP_DP3: return Format::ALU;
        case Opcode::OP_DP4: return Format::ALU;
        case Opcode::OP_RCP: return Format::ALU;
        case Opcode::OP_RSQ: return Format::ALU;
        case Opcode::OP_EXP: return Format::ALU;
        case Opcode::OP_LOG: return Format::ALU;
        case Opcode::OP_SIN: return Format::ALU;
        case Opcode::OP_COS: return Format::ALU;
        case Opcode::OP_MIN: return Format::ALU;
        case Opcode::OP_MAX: return Format::ALU;
        case Opcode::OP_ABS: return Format::ALU;
        case Opcode::OP_FRC: return Format::ALU;
        case Opcode::OP_FLR: return Format::ALU;
        case Opcode::OP_CMP: return Format::ALU;
        case Opcode::OP_BRA: return Format::FLOW;
        case Opcode::OP_CALL: return Format::FLOW;
        case Opcode::OP_RET: return Format::FLOW;
        case Opcode::OP_LOOP: return Format::FLOW;
        case Opcode::OP_ENDLOOP: return Format::FLOW;
        case Opcode::OP_BREAK: return Format::FLOW;
        case Opcode::OP_KIL: return Format::FLOW;
        case Opcode::OP_LD: return Format::MEM;
        case Opcode::OP_ST: return Format::MEM;
        case Opcode::OP_TEX: return Format::MEM;
        case Opcode::OP_TEXB: return Format::MEM;
        case Opcode::OP_TEXL: return Format::MEM;
        case Opcode::OP_TEXG: return Format::MEM;
        default: return Format::UNKNOWN;
    }
}

inline int opcode_operands(Opcode op) {
    switch (op) {
        case Opcode::OP_NOP: return 2;
        case Opcode::OP_MOV: return 2;
        case Opcode::OP_ADD: return 2;
        case Opcode::OP_MUL: return 2;
        case Opcode::OP_MAD: return 3;
        case Opcode::OP_DP3: return 2;
        case Opcode::OP_DP4: return 2;
        case Opcode::OP_RCP: return 2;
        case Opcode::OP_RSQ: return 2;
        case Opcode::OP_EXP: return 2;
        case Opcode::OP_LOG: return 2;
        case Opcode::OP_SIN: return 2;
        case Opcode::OP_COS: return 2;
        case Opcode::OP_MIN: return 2;
        case Opcode::OP_MAX: return 2;
        case Opcode::OP_ABS: return 2;
        case Opcode::OP_FRC: return 2;
        case Opcode::OP_FLR: return 2;
        case Opcode::OP_CMP: return 3;
        case Opcode::OP_BRA: return 2;
        case Opcode::OP_CALL: return 2;
        case Opcode::OP_RET: return 2;
        case Opcode::OP_LOOP: return 2;
        case Opcode::OP_ENDLOOP: return 2;
        case Opcode::OP_BREAK: return 2;
        case Opcode::OP_KIL: return 2;
        case Opcode::OP_LD: return 2;
        case Opcode::OP_ST: return 2;
        case Opcode::OP_TEX: return 2;
        case Opcode::OP_TEXB: return 2;
        case Opcode::OP_TEXL: return 2;
        case Opcode::OP_TEXG: return 2;
        default: return 0;
    }
}

}  // namespace gpu::isa
