#pragma once
// 64-bit instruction encoding for GPU shader ISA v1.0.
// Authoritative source is specs/isa.yaml — keep in sync.

#include <cstdint>

namespace gpu::isa {

using Inst = uint64_t;

// Source register classes
enum SrcClass : uint8_t {
    SRC_GPR     = 0b00,
    SRC_CONST   = 0b01,
    SRC_VARYING = 0b10,
};

// Predicate mode (2-bit)
enum PredMode : uint8_t {
    PMD_ALWAYS    = 0b00,
    PMD_IF_P      = 0b01,
    PMD_IF_NOT_P  = 0b10,
};

enum class Format { ALU, FLOW, MEM, UNKNOWN };

// Helpers — bit-pack/unpack utilities (no-magic via spec-derived constants).
constexpr inline uint64_t bits(uint64_t v, int hi, int lo) {
    return (v >> lo) & ((1ull << (hi - lo + 1)) - 1);
}
constexpr inline uint64_t put(uint64_t v, int hi, int lo) {
    return (v & ((1ull << (hi - lo + 1)) - 1)) << lo;
}

// ---------- ALU layout (see docs/isa_spec.md §3.1) ----------
// 63:58 op | 57 sat | 56:55 pmd | 54:50 dst | 49:46 wmsk
// 45:44 s0c | 43:39 s0idx | 38:31 sw0 | 30 n0 | 29 a0
// 28:27 s1c | 26:22 s1idx | 21:14 sw1 | 13 n1 | 12 a1
// 11:7 s2id | 6 n2 | 5 a2 | 4 dst_class | 3:0 reserved
//
// dst_class: 0 = GPR (dst[4:0] = r0..r31)
//            1 = output (dst[1:0] = o0..o3, dst[4:2] reserved)
struct AluFields {
    uint8_t op;        // 6 bits
    uint8_t sat;       // 1
    uint8_t pmd;       // 2
    uint8_t dst;       // 5
    uint8_t wmsk;      // 4
    uint8_t s0c;       // 2
    uint8_t s0idx;     // 5
    uint8_t sw0;       // 8
    uint8_t s0_neg;    // 1
    uint8_t s0_abs;    // 1
    uint8_t s1c;       // 2
    uint8_t s1idx;     // 5
    uint8_t sw1;       // 8
    uint8_t s1_neg;    // 1
    uint8_t s1_abs;    // 1
    uint8_t s2idx;     // 5  (GPR-only)
    uint8_t s2_neg;    // 1
    uint8_t s2_abs;    // 1
    uint8_t dst_class; // 1  (0 = GPR, 1 = output)
};

inline Inst encode_alu(const AluFields& f) {
    return put(f.op,        63, 58)
         | put(f.sat,       57, 57)
         | put(f.pmd,       56, 55)
         | put(f.dst,       54, 50)
         | put(f.wmsk,      49, 46)
         | put(f.s0c,       45, 44)
         | put(f.s0idx,     43, 39)
         | put(f.sw0,       38, 31)
         | put(f.s0_neg,    30, 30)
         | put(f.s0_abs,    29, 29)
         | put(f.s1c,       28, 27)
         | put(f.s1idx,     26, 22)
         | put(f.sw1,       21, 14)
         | put(f.s1_neg,    13, 13)
         | put(f.s1_abs,    12, 12)
         | put(f.s2idx,     11,  7)
         | put(f.s2_neg,     6,  6)
         | put(f.s2_abs,     5,  5)
         | put(f.dst_class,  4,  4);
}

inline AluFields decode_alu(Inst i) {
    AluFields f{};
    f.op        = bits(i, 63, 58);
    f.sat       = bits(i, 57, 57);
    f.pmd       = bits(i, 56, 55);
    f.dst       = bits(i, 54, 50);
    f.wmsk      = bits(i, 49, 46);
    f.s0c       = bits(i, 45, 44);
    f.s0idx     = bits(i, 43, 39);
    f.sw0       = bits(i, 38, 31);
    f.s0_neg    = bits(i, 30, 30);
    f.s0_abs    = bits(i, 29, 29);
    f.s1c       = bits(i, 28, 27);
    f.s1idx     = bits(i, 26, 22);
    f.sw1       = bits(i, 21, 14);
    f.s1_neg    = bits(i, 13, 13);
    f.s1_abs    = bits(i, 12, 12);
    f.s2idx     = bits(i, 11,  7);
    f.s2_neg    = bits(i,  6,  6);
    f.s2_abs    = bits(i,  5,  5);
    f.dst_class = bits(i,  4,  4);
    return f;
}

// ---------- Flow layout ----------
// 63:58 op | 57:56 pmd | 55 abs | 54:0 imm (signed)
struct FlowFields {
    uint8_t  op;
    uint8_t  pmd;
    uint8_t  abs_target;
    int64_t  imm;     // 55-bit signed
};
inline Inst encode_flow(const FlowFields& f) {
    return put(f.op,         63, 58)
         | put(f.pmd,         57, 56)
         | put(f.abs_target,  55, 55)
         | put(static_cast<uint64_t>(f.imm) & ((1ull<<55)-1), 54, 0);
}
inline FlowFields decode_flow(Inst i) {
    FlowFields f{};
    f.op = bits(i, 63, 58);
    f.pmd = bits(i, 57, 56);
    f.abs_target = bits(i, 55, 55);
    uint64_t imm_raw = bits(i, 54, 0);
    // sign-extend 55 -> 64
    if (imm_raw & (1ull << 54)) imm_raw |= ~((1ull << 55) - 1);
    f.imm = static_cast<int64_t>(imm_raw);
    return f;
}

// ---------- Memory / Texture layout ----------
// 63:58 op | 57:56 pmd | 55:51 dst | 50:47 wmsk | 46:42 src
// 41:34 src_swiz | 33:30 tex | 29:27 mode | 26:0 imm (signed)
struct MemFields {
    uint8_t  op;
    uint8_t  pmd;
    uint8_t  dst;
    uint8_t  wmsk;
    uint8_t  src;
    uint8_t  src_swiz;
    uint8_t  tex;
    uint8_t  mode;
    int32_t  imm;   // 27-bit signed
};
inline Inst encode_mem(const MemFields& f) {
    return put(f.op,          63, 58)
         | put(f.pmd,          57, 56)
         | put(f.dst,          55, 51)
         | put(f.wmsk,         50, 47)
         | put(f.src,          46, 42)
         | put(f.src_swiz,     41, 34)
         | put(f.tex,          33, 30)
         | put(f.mode,         29, 27)
         | put(static_cast<uint64_t>(f.imm) & ((1ull<<27)-1), 26, 0);
}
inline MemFields decode_mem(Inst i) {
    MemFields f{};
    f.op       = bits(i, 63, 58);
    f.pmd      = bits(i, 57, 56);
    f.dst      = bits(i, 55, 51);
    f.wmsk     = bits(i, 50, 47);
    f.src      = bits(i, 46, 42);
    f.src_swiz = bits(i, 41, 34);
    f.tex      = bits(i, 33, 30);
    f.mode     = bits(i, 29, 27);
    uint32_t imm_raw = bits(i, 26, 0);
    if (imm_raw & (1u << 26)) imm_raw |= 0xF8000000u;
    f.imm = static_cast<int32_t>(imm_raw);
    return f;
}

// ---------- Opcode → format classifier ----------
// Generated header has the exhaustive table; we re-derive here for convenience
// without a build-order dependency. Categories follow specs/isa.yaml.
inline Format format_of(uint8_t op) {
    if (op <= 0x13) return Format::ALU;       // nop..cmp
    if (op >= 0x18 && op <= 0x1D) return Format::ALU;   // setp_*
    if (op >= 0x20 && op <= 0x29) return Format::FLOW;  // bra..kil
    if (op == 0x30 || op == 0x31) return Format::MEM;
    if (op >= 0x34 && op <= 0x37) return Format::MEM;
    return Format::UNKNOWN;
}

// Swizzle helpers: 8-bit per-source, 2-bit per output component.
// component(sw, i) => which source channel (0..3) feeds destination component i.
constexpr inline uint8_t component(uint8_t sw, int i) {
    return (sw >> (i * 2)) & 0x3;
}
constexpr uint8_t SWIZZLE_IDENTITY = 0xE4;  // .xyzw

// Write mask helpers
constexpr inline bool wmsk_x(uint8_t m) { return m & 0x1; }
constexpr inline bool wmsk_y(uint8_t m) { return m & 0x2; }
constexpr inline bool wmsk_z(uint8_t m) { return m & 0x4; }
constexpr inline bool wmsk_w(uint8_t m) { return m & 0x8; }

// Output vs GPR destination: signaled by AluFields.dst_class (1 bit, stolen
// from reserved). dst[4:0] is the index in either case. Outputs use
// dst[1:0] (o0..o3); upper bits ignored.
constexpr inline uint8_t encode_dst_gpr(uint8_t r) { return r & 0x1F; }
constexpr inline uint8_t encode_dst_out(uint8_t o) { return o & 0x03; }

}  // namespace gpu::isa
