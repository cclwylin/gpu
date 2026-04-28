// Sprint-1 ISA simulator. Single-thread executor.
//
// Scope:
//  - All ALU 2-op + 3-op instructions covered by ISA v1.0
//  - All setp_* predicates
//  - if_p / else / endif (mask stack)
//  - loop / endloop / break (loop stack with iter count)
//  - kil (lane deactivation)
//  - tex (callback to host-provided sampler)
//
// Out of scope (Sprint 1):
//  - bra/call/ret with labels (no labelled assembler yet)
//  - texb/texl/texg modes are wired but use plain sample
//  - true 16-thread warp; we run one "thread"
//  - perf model

#include "gpu_compiler/sim.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "gpu_compiler/encoding.h"

namespace gpu::sim {
namespace {

using namespace gpu::isa;

// Apply 8-bit swizzle to a vec4.
Vec4 swizzle(const Vec4& v, uint8_t sw) {
    Vec4 r;
    for (int i = 0; i < 4; ++i) r[i] = v[component(sw, i)];
    return r;
}

Vec4 with_mods(Vec4 v, bool neg, bool abs_) {
    if (abs_) for (int i = 0; i < 4; ++i) v[i] = std::fabs(v[i]);
    if (neg)  for (int i = 0; i < 4; ++i) v[i] = -v[i];
    return v;
}

float ftz(float x) {
    uint32_t b; std::memcpy(&b, &x, 4);
    if (((b >> 23) & 0xFF) == 0) { b &= 0x80000000u; std::memcpy(&x, &b, 4); }
    return x;
}
Vec4 ftz4(Vec4 v) { for (int i = 0; i < 4; ++i) v[i] = ftz(v[i]); return v; }

float sat(float x) {
    if (x != x) return 0.0f;
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

const Vec4& read_src(const ThreadState& t, uint8_t cls, uint8_t idx) {
    static Vec4 zero{};
    switch (cls) {
        case SRC_GPR:     return idx < t.r.size()       ? t.r[idx]       : zero;
        case SRC_CONST:   return idx < t.c.size()       ? t.c[idx]       : zero;
        case SRC_VARYING: return idx < t.varying.size() ? t.varying[idx] : zero;
    }
    return zero;
}

Vec4 fetch_src(const ThreadState& t, uint8_t cls, uint8_t idx, uint8_t sw,
               bool neg, bool abs_) {
    return with_mods(swizzle(read_src(t, cls, idx), sw), neg, abs_);
}

Vec4& dst_lvalue(ThreadState& t, uint8_t dst, uint8_t dst_class) {
    static Vec4 sink{};
    if (dst_class) {
        // Sprint 58 — output index widened to 3 bits (o0..o7) so a VS
        // can drive the full 7 vec4 of varying capacity that
        // Vertex.varying[] already exposes.
        uint8_t i = dst & 0x7;
        return i < t.o.size() ? t.o[i] : sink;
    }
    return (dst & 0x1F) < t.r.size() ? t.r[dst & 0x1F] : sink;
}

void write_masked(Vec4& dst, const Vec4& src, uint8_t mask, bool sat_flag) {
    for (int i = 0; i < 4; ++i) {
        if (mask & (1 << i)) {
            float v = src[i];
            if (sat_flag) v = sat(v);
            dst[i] = ftz(v);
        }
    }
}

bool predicate_ok(uint8_t pmd, bool p) {
    if (pmd == PMD_ALWAYS) return true;
    if (pmd == PMD_IF_P)   return p;
    if (pmd == PMD_IF_NOT_P) return !p;
    return false;
}

// ---------------------------------------------------------------------------
// ALU implementations
// ---------------------------------------------------------------------------
Vec4 broadcast(float x) { return Vec4{{x, x, x, x}}; }

Vec4 op_add(Vec4 a, Vec4 b) { Vec4 r; for (int i=0;i<4;++i) r[i]=a[i]+b[i]; return r; }
Vec4 op_mul(Vec4 a, Vec4 b) { Vec4 r; for (int i=0;i<4;++i) r[i]=a[i]*b[i]; return r; }
Vec4 op_mad(Vec4 a, Vec4 b, Vec4 c) {
    Vec4 r; for (int i=0;i<4;++i) r[i]=std::fma(a[i],b[i],c[i]); return r;
}
Vec4 op_dpN(Vec4 a, Vec4 b, int n) {
    float s = 0.f; for (int i=0;i<n;++i) s += a[i]*b[i];
    return broadcast(s);
}
Vec4 op_min(Vec4 a, Vec4 b) { Vec4 r; for (int i=0;i<4;++i) r[i]=std::fmin(a[i],b[i]); return r; }
Vec4 op_max(Vec4 a, Vec4 b) { Vec4 r; for (int i=0;i<4;++i) r[i]=std::fmax(a[i],b[i]); return r; }
Vec4 op_abs(Vec4 a)         { Vec4 r; for (int i=0;i<4;++i) r[i]=std::fabs(a[i]); return r; }
Vec4 op_frc(Vec4 a)         { Vec4 r; for (int i=0;i<4;++i) r[i]=a[i]-std::floor(a[i]); return r; }
Vec4 op_flr(Vec4 a)         { Vec4 r; for (int i=0;i<4;++i) r[i]=std::floor(a[i]); return r; }
Vec4 op_cmp(Vec4 a, Vec4 b, Vec4 c) {
    Vec4 r; for (int i=0;i<4;++i) r[i] = a[i] >= 0.f ? b[i] : c[i]; return r;
}

bool setp_test(uint8_t op, float a, float b) {
    switch (op) {
        case 0x18: return a == b;
        case 0x19: return a != b;
        case 0x1A: return a <  b;
        case 0x1B: return a <= b;
        case 0x1C: return a >  b;
        case 0x1D: return a >= b;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Executor
// ---------------------------------------------------------------------------
ExecResult execute(const std::vector<Inst>& code, ThreadState& t, TexSampler tex) {
    ExecResult res;
    if (code.empty()) return res;

    // Mask stack for if_p / endif (simple bool semantics; one lane).
    std::vector<bool> mask_stack;
    bool active = true;

    // Loop stack: each entry holds (start_pc, remaining_iters).
    struct Loop { size_t start_pc; int64_t remaining; };
    std::vector<Loop> loop_stack;

    size_t pc = 0;
    while (pc < code.size()) {
        const Inst inst = code[pc];
        const uint8_t op = bits(inst, 63, 58);
        const Format fmt = format_of(op);

        bool advance = true;
        const bool is_alu_setp = (fmt == Format::ALU) && (op >= 0x18 && op <= 0x1D);

        if (fmt == Format::ALU) {
            AluFields f = decode_alu(inst);
            const bool runs = active && t.lane_active && predicate_ok(f.pmd, t.predicate);
            if (runs) {
                Vec4 s0 = fetch_src(t, f.s0c, f.s0idx, f.sw0, f.s0_neg, f.s0_abs);
                Vec4 s1 = fetch_src(t, f.s1c, f.s1idx, f.sw1, f.s1_neg, f.s1_abs);
                Vec4 s2 = with_mods(t.r[f.s2idx], f.s2_neg, f.s2_abs);
                Vec4 result;

                if (is_alu_setp) {
                    t.predicate = setp_test(f.op, s0[0], s1[0]);
                } else {
                    switch (f.op) {
                        case 0x00: result = Vec4{}; break;                  // nop
                        case 0x01: result = s0; break;                       // mov
                        case 0x02: result = op_add(s0, s1); break;
                        case 0x03: result = op_mul(s0, s1); break;
                        case 0x04: result = op_dpN(s0, s1, 2); break;
                        case 0x05: result = op_dpN(s0, s1, 3); break;
                        case 0x06: result = op_dpN(s0, s1, 4); break;
                        case 0x07: result = broadcast(ftz(1.0f / s0[0])); break;
                        case 0x08: result = broadcast(ftz(1.0f / std::sqrt(s0[0]))); break;
                        case 0x09: result = broadcast(ftz(std::exp2(s0[0]))); break;
                        case 0x0A: result = broadcast(ftz(std::log2(s0[0]))); break;
                        case 0x0B: result = broadcast(ftz(std::sin(s0[0]))); break;
                        case 0x0C: result = broadcast(ftz(std::cos(s0[0]))); break;
                        case 0x0D: result = op_min(s0, s1); break;
                        case 0x0E: result = op_max(s0, s1); break;
                        case 0x0F: result = op_abs(s0); break;
                        case 0x10: result = op_frc(s0); break;
                        case 0x11: result = op_flr(s0); break;
                        case 0x12: result = op_mad(s0, s1, s2); break;
                        case 0x13: result = op_cmp(s0, s1, s2); break;
                        default:
                            res.ok = false;
                            res.error = "unimplemented ALU op";
                            return res;
                    }
                    write_masked(dst_lvalue(t, f.dst, f.dst_class), ftz4(result),
                                 f.wmsk, f.sat != 0);
                }
            }
        } else if (fmt == Format::FLOW) {
            FlowFields f = decode_flow(inst);
            const bool predicated_run = predicate_ok(f.pmd, t.predicate);

            switch (f.op) {
                case 0x23: {  // loop <imm>
                    if (active && t.lane_active) {
                        loop_stack.push_back({pc + 1, f.imm});
                    }
                    break;
                }
                case 0x24: {  // endloop
                    if (!loop_stack.empty()) {
                        auto& L = loop_stack.back();
                        L.remaining -= 1;
                        if (L.remaining > 0 && active && t.lane_active) {
                            pc = L.start_pc;
                            advance = false;
                        } else {
                            loop_stack.pop_back();
                        }
                    }
                    break;
                }
                case 0x25: {  // break (predicated)
                    if (active && t.lane_active && predicated_run && !loop_stack.empty()) {
                        // Skip forward to matching endloop.
                        int depth = 1;
                        size_t p = pc + 1;
                        while (p < code.size() && depth > 0) {
                            uint8_t pop = bits(code[p], 63, 58);
                            if (pop == 0x23) ++depth;
                            else if (pop == 0x24) --depth;
                            ++p;
                        }
                        loop_stack.pop_back();
                        pc = p;
                        advance = false;
                    }
                    break;
                }
                case 0x26: {  // if_p — push current active and AND with predicate
                    mask_stack.push_back(active);
                    active = active && t.predicate;
                    break;
                }
                case 0x27: {  // else — flip masked lanes that were active before push
                    if (!mask_stack.empty()) {
                        bool prev = mask_stack.back();
                        active = prev && !active;   // simple lane semantics
                    }
                    break;
                }
                case 0x28: {  // endif — pop
                    if (!mask_stack.empty()) {
                        active = mask_stack.back();
                        mask_stack.pop_back();
                    }
                    break;
                }
                case 0x29: {  // kil
                    if (active && predicated_run) t.lane_active = false;
                    break;
                }
                case 0x22: {  // ret — only meaningful with call stack; treat as halt
                    return res;
                }
                default:
                    res.ok = false;
                    res.error = "unimplemented flow op";
                    return res;
            }
        } else if (fmt == Format::MEM) {
            MemFields f = decode_mem(inst);
            const bool runs = active && t.lane_active && predicate_ok(f.pmd, t.predicate);
            if (runs) {
                if (f.op == 0x34 || f.op == 0x35 || f.op == 0x36 || f.op == 0x37) {
                    if (!tex) { res.ok = false; res.error = "tex op without sampler"; return res; }
                    Vec4 src_val = swizzle(read_src(t, f.src_class, f.src), f.src_swiz);
                    Vec4 sample  = tex(f.tex, src_val, f.mode, src_val[3]);
                    Vec4& dst    = dst_lvalue(t, f.dst, f.dst_class);
                    write_masked(dst, sample, f.wmsk, /*sat=*/false);
                } else {
                    // ld/st: not exercised by ref shaders yet. No-op safe.
                }
            }
        }

        if (advance) ++pc;
    }
    return res;
}

}  // namespace gpu::sim
