// Sprint-6 16-thread warp executor.
//
// Adds per-lane execution mask + mask stack to the existing single-thread
// dispatch. Each ALU / MEM op runs only on lanes where the active mask = 1.
// `setp_*` writes WarpState.predicate per lane. `if_p / else / endif` and
// `loop / break / endloop` push/pop the mask. `kil` permanently clears the
// affected lane's lane_active flag.
//
// Re-uses the per-instruction ALU semantics from sim.cpp by using the same
// helper layout; we keep the code self-contained here for clarity rather
// than refactoring sim.cpp into a giant shared module — Phase 1.x can merge.

#include "gpu_compiler/sim.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace gpu::sim {
namespace {

using namespace gpu::isa;

inline Vec4 swizzle(const Vec4& v, uint8_t sw) {
    Vec4 r;
    for (int i = 0; i < 4; ++i) r[i] = v[(sw >> (i * 2)) & 0x3];
    return r;
}

inline Vec4 with_mods(Vec4 v, bool neg, bool abs_) {
    if (abs_) for (int i = 0; i < 4; ++i) v[i] = std::fabs(v[i]);
    if (neg)  for (int i = 0; i < 4; ++i) v[i] = -v[i];
    return v;
}

inline float ftz(float x) {
    uint32_t b; std::memcpy(&b, &x, 4);
    if (((b >> 23) & 0xFF) == 0) { b &= 0x80000000u; std::memcpy(&x, &b, 4); }
    return x;
}
inline Vec4 ftz4(Vec4 v) { for (int i = 0; i < 4; ++i) v[i] = ftz(v[i]); return v; }

inline float sat(float x) {
    if (x != x) return 0.0f;
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

const Vec4& read_src_lane(const ThreadState& t, uint8_t cls, uint8_t idx) {
    static Vec4 zero{};
    switch (cls) {
        case SRC_GPR:     return idx < t.r.size()       ? t.r[idx]       : zero;
        case SRC_CONST:   return idx < t.c.size()       ? t.c[idx]       : zero;
        case SRC_VARYING: return idx < t.varying.size() ? t.varying[idx] : zero;
    }
    return zero;
}

Vec4& dst_lvalue_lane(ThreadState& t, uint8_t dst, uint8_t dst_class) {
    static Vec4 sink{};
    if (dst_class) {
        uint8_t i = dst & 0x3;
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

inline Vec4 broadcast(float x) { return Vec4{{x, x, x, x}}; }
inline Vec4 add_(Vec4 a, Vec4 b){ Vec4 r;for(int i=0;i<4;++i)r[i]=a[i]+b[i];return r; }
inline Vec4 mul_(Vec4 a, Vec4 b){ Vec4 r;for(int i=0;i<4;++i)r[i]=a[i]*b[i];return r; }
inline Vec4 mad_(Vec4 a, Vec4 b, Vec4 c){ Vec4 r;for(int i=0;i<4;++i)r[i]=std::fma(a[i],b[i],c[i]);return r; }
inline Vec4 dpN(Vec4 a, Vec4 b, int n){
    float s=0; for (int i=0;i<n;++i) s += a[i]*b[i];
    return broadcast(s);
}
inline Vec4 min_(Vec4 a, Vec4 b){ Vec4 r;for(int i=0;i<4;++i)r[i]=std::fmin(a[i],b[i]);return r; }
inline Vec4 max_(Vec4 a, Vec4 b){ Vec4 r;for(int i=0;i<4;++i)r[i]=std::fmax(a[i],b[i]);return r; }
inline Vec4 abs_v(Vec4 a){ Vec4 r;for(int i=0;i<4;++i)r[i]=std::fabs(a[i]);return r; }
inline Vec4 frc_(Vec4 a){ Vec4 r;for(int i=0;i<4;++i)r[i]=a[i]-std::floor(a[i]);return r; }
inline Vec4 flr_(Vec4 a){ Vec4 r;for(int i=0;i<4;++i)r[i]=std::floor(a[i]);return r; }
inline Vec4 cmp_(Vec4 a, Vec4 b, Vec4 c){
    Vec4 r;for(int i=0;i<4;++i) r[i] = a[i]>=0.f ? b[i] : c[i]; return r;
}

bool setp_test(uint8_t op, float a, float b) {
    switch (op) {
        case 0x18: return a == b; case 0x19: return a != b;
        case 0x1A: return a <  b; case 0x1B: return a <= b;
        case 0x1C: return a >  b; case 0x1D: return a >= b;
    }
    return false;
}

inline bool pmd_ok_lane(uint8_t pmd, uint16_t pred, int lane) {
    if (pmd == PMD_ALWAYS)   return true;
    bool p = (pred >> lane) & 1u;
    if (pmd == PMD_IF_P)     return p;
    if (pmd == PMD_IF_NOT_P) return !p;
    return false;
}

// Execute one ALU instruction across all lanes that have active=1.
void exec_alu_warp(const AluFields& f, WarpState& w, uint16_t active_mask) {
    for (int lane = 0; lane < kWarpSize; ++lane) {
        if (!((active_mask >> lane) & 1u)) continue;
        ThreadState& t = w.lane[lane];
        if (!t.lane_active) continue;
        if (!pmd_ok_lane(f.pmd, w.predicate, lane)) continue;

        Vec4 s0 = with_mods(swizzle(read_src_lane(t, f.s0c, f.s0idx), f.sw0),
                            f.s0_neg, f.s0_abs);
        Vec4 s1 = with_mods(swizzle(read_src_lane(t, f.s1c, f.s1idx), f.sw1),
                            f.s1_neg, f.s1_abs);
        Vec4 s2 = with_mods(t.r[f.s2idx], f.s2_neg, f.s2_abs);
        Vec4 result;

        const bool is_setp = (f.op >= 0x18 && f.op <= 0x1D);
        if (is_setp) {
            bool b = setp_test(f.op, s0[0], s1[0]);
            uint16_t bit = static_cast<uint16_t>(1u << lane);
            if (b) w.predicate |=  bit;
            else   w.predicate &= ~bit;
            continue;
        }

        switch (f.op) {
            case 0x00: result = Vec4{}; break;
            case 0x01: result = s0; break;
            case 0x02: result = add_(s0, s1); break;
            case 0x03: result = mul_(s0, s1); break;
            case 0x04: result = dpN(s0, s1, 2); break;
            case 0x05: result = dpN(s0, s1, 3); break;
            case 0x06: result = dpN(s0, s1, 4); break;
            case 0x07: result = broadcast(ftz(1.0f / s0[0])); break;
            case 0x08: result = broadcast(ftz(1.0f / std::sqrt(s0[0]))); break;
            case 0x09: result = broadcast(ftz(std::exp2(s0[0]))); break;
            case 0x0A: result = broadcast(ftz(std::log2(s0[0]))); break;
            case 0x0B: result = broadcast(ftz(std::sin(s0[0]))); break;
            case 0x0C: result = broadcast(ftz(std::cos(s0[0]))); break;
            case 0x0D: result = min_(s0, s1); break;
            case 0x0E: result = max_(s0, s1); break;
            case 0x0F: result = abs_v(s0); break;
            case 0x10: result = frc_(s0); break;
            case 0x11: result = flr_(s0); break;
            case 0x12: result = mad_(s0, s1, s2); break;
            case 0x13: result = cmp_(s0, s1, s2); break;
            default:   continue;
        }
        write_masked(dst_lvalue_lane(t, f.dst, f.dst_class), ftz4(result),
                     f.wmsk, f.sat != 0);
    }
}

}  // namespace

ExecResult execute_warp(const std::vector<Inst>& code, WarpState& w, TexSampler tex) {
    ExecResult res;
    if (code.empty()) return res;

    // Mask stack — one global value (the active mask itself) per push.
    // For if_p we push the current active and AND with the per-lane predicate;
    // else flips, endif pops.
    std::vector<uint16_t> mask_stack;
    uint16_t active = static_cast<uint16_t>((1u << kWarpSize) - 1u);

    // Loop frame holds the active mask at loop entry so endloop / all-lanes-
    // broken can restore it (per-lane reconvergence: lanes that `break` clear
    // their bit from the live `active` but the loop continues for others).
    struct Loop { size_t start_pc; int64_t remaining; uint16_t entry_active; };
    std::vector<Loop> loop_stack;

    size_t pc = 0;
    while (pc < code.size()) {
        const Inst inst = code[pc];
        const uint8_t op = bits(inst, 63, 58);
        const Format fmt = format_of(op);

        bool advance = true;

        if (fmt == Format::ALU) {
            AluFields f = decode_alu(inst);
            exec_alu_warp(f, w, active);
        } else if (fmt == Format::FLOW) {
            FlowFields f = decode_flow(inst);

            switch (f.op) {
                case 0x23: {  // loop — push frame, capture entry active mask
                    loop_stack.push_back({pc + 1, f.imm, active});
                    break;
                }
                case 0x24: {  // endloop
                    if (!loop_stack.empty()) {
                        auto& L = loop_stack.back();
                        L.remaining -= 1;
                        if (L.remaining > 0 && active != 0) {
                            pc = L.start_pc; advance = false;
                        } else {
                            active = L.entry_active;     // restore on natural exit
                            loop_stack.pop_back();
                        }
                    }
                    break;
                }
                case 0x25: {  // break — per-lane: clear lanes from live active
                    if (loop_stack.empty()) break;
                    uint16_t break_mask = 0;
                    for (int lane = 0; lane < kWarpSize; ++lane) {
                        if (!((active >> lane) & 1u)) continue;
                        if (pmd_ok_lane(f.pmd, w.predicate, lane)) {
                            break_mask |= static_cast<uint16_t>(1u << lane);
                        }
                    }
                    active = static_cast<uint16_t>(active & ~break_mask);
                    if (active == 0) {
                        // All in-loop lanes broke — skip past matching endloop
                        // and restore the pre-loop active mask.
                        int depth = 1; size_t p = pc + 1;
                        while (p < code.size() && depth > 0) {
                            uint8_t pop = bits(code[p], 63, 58);
                            if (pop == 0x23) ++depth;
                            else if (pop == 0x24) --depth;
                            ++p;
                        }
                        active = loop_stack.back().entry_active;
                        loop_stack.pop_back();
                        pc = p; advance = false;
                    }
                    break;
                }
                case 0x26: {  // if_p — push, mask &= predicate
                    mask_stack.push_back(active);
                    active = active & w.predicate;
                    break;
                }
                case 0x27: {  // else — keep only previously-active lanes that
                              // were NOT in the if branch.
                    if (!mask_stack.empty()) {
                        uint16_t prev = mask_stack.back();
                        active = static_cast<uint16_t>(prev & ~active);
                    }
                    break;
                }
                case 0x28: {  // endif
                    if (!mask_stack.empty()) {
                        active = mask_stack.back();
                        mask_stack.pop_back();
                    }
                    break;
                }
                case 0x29: {  // kil — clear lane_active for predicated lanes
                    for (int lane = 0; lane < kWarpSize; ++lane) {
                        if (!((active >> lane) & 1u)) continue;
                        if (pmd_ok_lane(f.pmd, w.predicate, lane)) {
                            w.lane[lane].lane_active = false;
                        }
                    }
                    break;
                }
                case 0x22:    // ret — halt
                    return res;
                default:
                    res.ok = false; res.error = "unimpl flow op"; return res;
            }
        } else if (fmt == Format::MEM) {
            MemFields f = decode_mem(inst);
            if (f.op == 0x34 || f.op == 0x35 || f.op == 0x36 || f.op == 0x37) {
                if (!tex) { res.ok = false; res.error = "tex without sampler"; return res; }
                for (int lane = 0; lane < kWarpSize; ++lane) {
                    if (!((active >> lane) & 1u)) continue;
                    ThreadState& t = w.lane[lane];
                    if (!t.lane_active) continue;
                    if (!pmd_ok_lane(f.pmd, w.predicate, lane)) continue;
                    Vec4 src_val = swizzle(read_src_lane(t, f.src_class, f.src),
                                           f.src_swiz);
                    Vec4 sample  = tex(f.tex, src_val, f.mode, src_val[3]);
                    Vec4& dst    = dst_lvalue_lane(t, f.dst, f.dst_class);
                    write_masked(dst, sample, f.wmsk, false);
                }
            }
            // ld/st: not yet exercised.
        }

        if (advance) ++pc;
    }
    return res;
}

}  // namespace gpu::sim
