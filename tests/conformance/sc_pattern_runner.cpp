// Sprint 34 — SystemC pattern runner.
//
// Reads a .scene file (same format as scene_runner.cpp), feeds the
// vertices through the cycle-accurate chain
//
//   src → SC_ca → SC_to_PA → PA_ca → PA_to_RS → RS_ca →
//         RS_to_PFO → PFO_ca → PFO_to_TBF → TBF_ca → RSV_ca → sink
//
// dumps the resulting fb.color as a binary P6 PPM, and prints
// `CYCLES=<n>` on stdout in a script-parseable form.
//
// Usage:
//   sc_pattern_runner <scene_file> <output.ppm>
//                     [--ref <ref.ppm> --rmse-max <float>]
//
// When --ref is given, the rendered fb is compared to <ref.ppm> and the
// RMSE is printed as `RMSE=<n>`. If --rmse-max is also supplied, the
// runner exits non-zero when RMSE exceeds the budget — this is what the
// `conformance.<name>.sc` ctest entries use to assert SC ↔ sw_ref
// bit-parity against the golden PPM under tests/scenes/.
//
// Docker-only on macOS+GCC due to the libstdc++/libc++ ABI mismatch
// in /usr/local/systemc-2.3.4 (same gate as the other systemc tests).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

#include "gpu_systemc/pa_to_rs_adapter_ca.h"
#include "gpu_systemc/perfragmentops_ca.h"
#include "gpu_systemc/pfo_to_tbf_adapter_ca.h"
#include "gpu_systemc/primitiveassembly_ca.h"
#include "gpu_systemc/rasterizer_ca.h"
#include "gpu_systemc/resolveunit_ca.h"
#include "gpu_systemc/rs_to_pfo_adapter_ca.h"
#include "gpu_systemc/sc_to_pa_adapter_ca.h"
#include "gpu_systemc/shadercore_ca.h"
#include "gpu_systemc/tilebuffer_ca.h"

using namespace gpu::systemc;

namespace {

// One draw batch with its own render-state snapshot. A scene is an
// ordered sequence of ops — BATCH (pipeline draw) and CLEAR_RECT
// (mid-frame fb wipe; texenv etc. lean on this for per-cell glClear
// to reproduce). Each batch sees the SC chain with the depth/cull/
// blend state that was active in glcompat at capture time so mid-
// frame state toggles (movelight's torus + bitmap-font overlay, etc.)
// reproduce correctly.
struct Batch {
    bool depth_test = false;
    bool depth_write = true;
    std::string depth_func = "less";
    bool cull_back = false;
    bool blend = false;
    // Sprint 61 — full blend state. apply_batch_state maps the
    // string-shaped factors / equations onto gpu::DrawState's BF_* /
    // BE_* enums. Defaults match GLES 2.0 / DrawState defaults so
    // older scenes (no `blend_func` / `blend_eq` keyword) still work.
    std::string blend_src_rgb   = "src_alpha";
    std::string blend_dst_rgb   = "one_minus_src_alpha";
    std::string blend_src_alpha = "src_alpha";
    std::string blend_dst_alpha = "one_minus_src_alpha";
    std::string blend_eq_rgb    = "add";
    std::string blend_eq_alpha  = "add";
    float       blend_color[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
    // Sprint 61 — per-batch viewport. 0/0 = "use scene-level fb size"
    // (legacy default). When set, the SC chain's PA gets vp_x/vp_y/w/h
    // for this batch and renders into the matching sub-rect.
    int         vp_x = 0, vp_y = 0, vp_w = 0, vp_h = 0;
    bool stencil_test = false;
    std::string stencil_func = "always";
    int  stencil_ref  = 0;
    int  stencil_read_mask  = 0xFF;
    int  stencil_write_mask = 0xFF;
    std::string stencil_sfail  = "keep";
    std::string stencil_dpfail = "keep";
    std::string stencil_dppass = "keep";
    // Sprint 61 — per-vertex N varyings (was a single `colours` slot
    // before). `n_vars` mirrors the writer's `varying_count` keyword
    // (default 1 = legacy single-varying form). Each entry in
    // `varyings` is a 7-slot array; only the first `n_vars` slots are
    // populated. The pa_rs.varying_count uses the same N.
    int n_vars = 1;
    std::vector<gpu::Vec4f> positions;
    std::vector<std::array<gpu::Vec4f, 7>> varyings;
};

struct BitmapOp {
    int      x = 0, y = 0, w = 0, h = 0;
    uint32_t color = 0;
    std::vector<uint8_t> bits;
};
struct SceneOp {
    enum Kind { BATCH, CLEAR, BITMAP, CLEAR_DEPTH, CLEAR_STENCIL } kind = BATCH;
    Batch    batch;
    uint32_t clear_rgba = 0;
    BitmapOp bitmap;
    float    clear_depth = 1.0f;
    int      clear_stencil_val = 0;
    // Sprint 60 — per-CLEAR scissor rect + 32-bit color-mask lane.
    // `clear_rect_full=true` is the legacy whole-fb form; the parser
    // sets it for the bare `clear_rect <rgba>` line and flips it when
    // the extended `clear_rect <rgba> x0 y0 x1 y1 lane` line is read.
    bool     clear_rect_full = true;
    int      clear_x0 = 0, clear_y0 = 0, clear_x1 = 0, clear_y1 = 0;
    uint32_t clear_lane = 0xFFFFFFFFu;
};

struct Scene {
    int width = 32;
    int height = 32;
    bool msaa = false;
    uint32_t clear_rgba = 0;
    std::vector<SceneOp> ops;
};

bool parse_scene(const std::string& path, Scene& s, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open " + path; return false; }
    Batch defaults;
    Batch* cur = nullptr;     // active explicit `batch` block (or nullptr)
    Batch  legacy;            // implicit batch built from top-level verts
    bool   has_legacy_verts = false;
    bool   in_verts = false;
    std::string line;
    while (std::getline(f, line)) {
        const size_t fnw = line.find_first_not_of(" \t");
        if (fnw == std::string::npos || line[fnw] == '#') continue;
        if (in_verts) {
            std::istringstream is(line);
            std::string first; is >> first;
            if (first == "end" || first == "end_verts") { in_verts = false; continue; }
            // Sprint 61 — vert line: 4 (pos) + 4·N (varying) floats.
            // `tgt->n_vars` is set by the surrounding `varying_count`
            // keyword (defaults to 1 for legacy scenes).
            std::istringstream is2(line);
            float x, y, z, w;
            if (!(is2 >> x >> y >> z >> w)) {
                err = "bad vertex line: " + line; return false;
            }
            Batch* tgt = cur ? cur : (has_legacy_verts ? &legacy : (legacy = defaults, has_legacy_verts = true, &legacy));
            std::array<gpu::Vec4f, 7> vrow{};
            const int nv = std::max(1, std::min(7, tgt->n_vars));
            for (int k = 0; k < nv; ++k) {
                float r, g, b, a;
                if (!(is2 >> r >> g >> b >> a)) {
                    err = "bad vertex line (varying " + std::to_string(k) + "): " + line;
                    return false;
                }
                vrow[k] = {{r, g, b, a}};
            }
            tgt->positions.push_back({{x, y, z, w}});
            tgt->varyings.push_back(vrow);
            continue;
        }
        std::istringstream is(line);
        std::string key; is >> key;
        if (key == "batch") {
            SceneOp op; op.kind = SceneOp::BATCH; op.batch = defaults;
            s.ops.push_back(std::move(op));
            cur = &s.ops.back().batch;
            continue;
        }
        if (key == "end_batch") { cur = nullptr; continue; }
        if (key == "clear_rect") {
            uint32_t v; is >> std::hex >> v >> std::dec;
            SceneOp op; op.kind = SceneOp::CLEAR; op.clear_rgba = v;
            // Sprint 60 — optional extended form:
            //   clear_rect <rgba> <x0> <y0> <x1> <y1> <lane>
            int x0, y0, x1, y1; uint32_t lane;
            if (is >> x0 >> y0 >> x1 >> y1 >> std::hex >> lane >> std::dec) {
                op.clear_rect_full = false;
                op.clear_x0 = x0; op.clear_y0 = y0;
                op.clear_x1 = x1; op.clear_y1 = y1;
                op.clear_lane = lane;
            }
            s.ops.push_back(std::move(op));
            continue;
        }
        if (key == "clear_depth") {
            float v; is >> v;
            SceneOp op; op.kind = SceneOp::CLEAR_DEPTH; op.clear_depth = v;
            s.ops.push_back(std::move(op));
            continue;
        }
        if (key == "clear_stencil") {
            int v; is >> v;
            SceneOp op; op.kind = SceneOp::CLEAR_STENCIL; op.clear_stencil_val = v;
            s.ops.push_back(std::move(op));
            continue;
        }
        if (key == "bitmap") {
            int bx, by, bw, bh; uint32_t bc;
            std::string hex;
            if (!(is >> bx >> by >> bw >> bh >> std::hex >> bc >> hex)) {
                err = "bad bitmap line"; return false;
            }
            SceneOp op; op.kind = SceneOp::BITMAP;
            op.bitmap.x = bx; op.bitmap.y = by;
            op.bitmap.w = bw; op.bitmap.h = bh;
            op.bitmap.color = bc;
            op.bitmap.bits.reserve(hex.size() / 2);
            for (size_t i = 0; i + 1 < hex.size(); i += 2) {
                auto hexv = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                op.bitmap.bits.push_back(
                    (uint8_t)((hexv(hex[i]) << 4) | hexv(hex[i + 1])));
            }
            s.ops.push_back(std::move(op));
            continue;
        }
        if (key == "width")  { is >> s.width;  continue; }
        if (key == "height") { is >> s.height; continue; }
        if (key == "msaa")   { int v; is >> v; s.msaa = (v != 0); continue; }
        if (key == "clear")  { uint32_t v; is >> std::hex >> v >> std::dec; s.clear_rgba = v; continue; }
        Batch* tgt = cur ? cur : &defaults;
        if      (key == "depth_test")  { int v; is >> v; tgt->depth_test  = v != 0; }
        else if (key == "depth_write") { int v; is >> v; tgt->depth_write = v != 0; }
        else if (key == "depth_func")  { is >> tgt->depth_func; }
        else if (key == "cull_back")   { int v; is >> v; tgt->cull_back   = v != 0; }
        else if (key == "blend")       { int v; is >> v; tgt->blend       = v != 0; }
        else if (key == "stencil_test"){ int v; is >> v; tgt->stencil_test = v != 0; }
        else if (key == "stencil_func") {
            is >> tgt->stencil_func >> tgt->stencil_ref >> tgt->stencil_read_mask;
        }
        else if (key == "stencil_op") {
            is >> tgt->stencil_sfail >> tgt->stencil_dpfail >> tgt->stencil_dppass;
        }
        else if (key == "stencil_write_mask") { is >> tgt->stencil_write_mask; }
        // Sprint 61 — full blend state.
        else if (key == "blend_func") {
            is >> tgt->blend_src_rgb >> tgt->blend_dst_rgb
               >> tgt->blend_src_alpha >> tgt->blend_dst_alpha;
        }
        else if (key == "blend_eq") {
            is >> tgt->blend_eq_rgb >> tgt->blend_eq_alpha;
        }
        else if (key == "blend_color") {
            is >> tgt->blend_color[0] >> tgt->blend_color[1]
               >> tgt->blend_color[2] >> tgt->blend_color[3];
        }
        else if (key == "viewport") {
            is >> tgt->vp_x >> tgt->vp_y >> tgt->vp_w >> tgt->vp_h;
        }
        // Sprint 61 — `varying_count N` (N in 1..7) before `verts` so
        // each vert line carries 4 + 4·N floats. Older scenes omit this
        // keyword and stay at the implicit N=1.
        else if (key == "varying_count") {
            int n; if (is >> n) tgt->n_vars = std::max(1, std::min(7, n));
        }
        else if (key == "verts")       { in_verts = true; }
    }
    if (has_legacy_verts) {
        SceneOp op; op.kind = SceneOp::BATCH; op.batch = std::move(legacy);
        s.ops.push_back(std::move(op));
    }
    for (const auto& op : s.ops) {
        if (op.kind == SceneOp::BATCH && op.batch.positions.size() % 3 != 0) {
            err = "batch vertex count must be a multiple of 3";
            return false;
        }
    }
    return true;
}

bool write_ppm(const std::string& path, const std::vector<uint32_t>& fb,
               int W, int H) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            uint32_t c = fb[static_cast<size_t>(y) * W + x];
            f.put(static_cast<char>(c & 0xFF));
            f.put(static_cast<char>((c >>  8) & 0xFF));
            f.put(static_cast<char>((c >> 16) & 0xFF));
        }
    }
    return true;
}

bool read_ppm(const std::string& path, std::vector<uint32_t>& out,
              int& W, int& H, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    std::string magic;
    int maxv = 0;
    f >> magic >> W >> H >> maxv;
    if (magic != "P6" || maxv != 255) {
        err = "unsupported PPM format (need P6 maxval 255)"; return false;
    }
    f.get();
    out.assign(static_cast<size_t>(W) * H, 0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            unsigned char rgb[3];
            f.read(reinterpret_cast<char*>(rgb), 3);
            if (!f) { err = "PPM truncated"; return false; }
            uint32_t v = static_cast<uint32_t>(rgb[0])
                       | (static_cast<uint32_t>(rgb[1]) <<  8)
                       | (static_cast<uint32_t>(rgb[2]) << 16)
                       | (0xFFu << 24);
            out[static_cast<size_t>(y) * W + x] = v;
        }
    }
    return true;
}

float rmse(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return 1e9f;
    double sum_sq = 0.0;
    const size_t N = a.size();
    for (size_t i = 0; i < N; ++i) {
        for (int ch = 0; ch < 3; ++ch) {
            const float av = static_cast<float>((a[i] >> (ch * 8)) & 0xFF);
            const float bv = static_cast<float>((b[i] >> (ch * 8)) & 0xFF);
            sum_sq += static_cast<double>((av - bv) * (av - bv));
        }
    }
    return static_cast<float>(std::sqrt(sum_sq / static_cast<double>(N * 3)));
}

}  // namespace

SC_MODULE(Source) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;
    sc_core::sc_out<bool>     valid;
    sc_core::sc_in<bool>      ready;
    sc_core::sc_out<uint64_t> data;
    std::queue<uint64_t> q;
    void push(uint64_t v) { q.push(v); }
    SC_HAS_PROCESS(Source);
    explicit Source(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        valid.write(false); data.write(0);
        while (true) {
            if (q.empty()) { valid.write(false); wait(); continue; }
            valid.write(true); data.write(q.front());
            wait();
            while (!ready.read()) wait();
            q.pop();
            valid.write(false); wait();
        }
    }
};

SC_MODULE(Sink) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;
    sc_core::sc_in<bool>      valid;
    sc_core::sc_out<bool>     ready;
    sc_core::sc_in<uint64_t>  data;
    int seen = 0;
    SC_HAS_PROCESS(Sink);
    explicit Sink(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        ready.write(false);
        while (true) { ready.write(true); wait();
            if (valid.read()) ++seen; }
    }
};

int sc_main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <scene_file> <output.ppm> "
                     "[--ref <ref.ppm> --rmse-max <float>]\n", argv[0]);
        return 2;
    }
    const std::string scene_path = argv[1];
    const std::string out_ppm    = argv[2];
    std::string ref_ppm;
    float rmse_max = -1.0f;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ref") == 0 && i + 1 < argc) {
            ref_ppm = argv[++i];
        } else if (std::strcmp(argv[i], "--rmse-max") == 0 && i + 1 < argc) {
            rmse_max = std::stof(argv[++i]);
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", argv[i]); return 2;
        }
    }

    Scene scene;
    std::string err;
    if (!parse_scene(scene_path, scene, err)) {
        std::fprintf(stderr, "parse: %s\n", err.c_str()); return 1;
    }

    // Optional cap: TBF/RSV placeholders cost O(tile_w × tile_h) cycles
    // per flush, so high-res chains take minutes. SC_FB_MAX scales the
    // framebuffer (and the scene's NDC-derived screen-space coords are
    // unaffected — viewport is set per-block to the new dims).
    if (const char* e = std::getenv("SC_FB_MAX")) {
        const int cap = std::atoi(e);
        if (cap > 0 && (scene.width > cap || scene.height > cap)) {
            const float sx = (float)cap / scene.width;
            const float sy = (float)cap / scene.height;
            const float s = std::min(sx, sy);
            scene.width  = (int)(scene.width  * s);
            scene.height = (int)(scene.height * s);
            std::fprintf(stderr, "[sc] downscaled to %dx%d (SC_FB_MAX=%d)\n",
                         scene.width, scene.height, cap);
        }
    }

    // ---- VS: copy c0→o0 (position), c1..cN→o1..oN (N varyings) ----
    // Sprint 61 — per-batch N varyings. Each vertex's constants slot
    // ck (for k in 0..N) carries pos at k=0 and varying[k-1] otherwise.
    // We assemble a VS that mov's all (max N seen + 1) outputs; the
    // per-vertex constant load below leaves the unused slots zero,
    // which is fine because the rasterizer's `varying_count` clips
    // downstream consumption to the active N.
    int max_n_vars = 1;
    for (const auto& op : scene.ops)
        if (op.kind == SceneOp::BATCH)
            max_n_vars = std::max(max_n_vars, op.batch.n_vars);
    std::string vs_src = "mov o0, c0\n";
    for (int k = 1; k <= max_n_vars; ++k) {
        vs_src += "mov o" + std::to_string(k) + ", c"
                + std::to_string(k) + "\n";
    }
    auto a = gpu::asm_::assemble(vs_src);
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str()); return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    // ---- Wire up CA chain ----
    sc_core::sc_clock           clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool>    rst_n;

    #define DECL_HOP(name) \
        sc_core::sc_signal<bool>     name##_valid, name##_ready; \
        sc_core::sc_signal<uint64_t> name##_data
    DECL_HOP(s0); DECL_HOP(s1); DECL_HOP(s2); DECL_HOP(s3); DECL_HOP(s4);
    DECL_HOP(s5); DECL_HOP(s6); DECL_HOP(s7); DECL_HOP(s8); DECL_HOP(s9);
    DECL_HOP(s10);
    #undef DECL_HOP

    Source              src    ("src");
    ShaderCoreCa        sc_    ("sc");
    ScToPaAdapterCa     sc_pa  ("sc_pa");
    PrimitiveAssemblyCa pa     ("pa");
    PaToRsAdapterCa     pa_rs  ("pa_rs");
    RasterizerCa        rs     ("rs");
    RsToPfoAdapterCa    rs_pfo ("rs_pfo");
    PerFragmentOpsCa    pfo    ("pfo");
    PfoToTbfAdapterCa   pfo_tbf("pfo_tbf");
    TileBufferCa        tbf    ("tbf");
    ResolveUnitCa       rsv    ("rsv");
    Sink                sink   ("sink");

    gpu::Context ctx;
    ctx.fb.width = scene.width; ctx.fb.height = scene.height;
    ctx.fb.msaa_4x = scene.msaa;
    ctx.fb.color.assign(scene.width * scene.height, scene.clear_rgba);
    if (scene.msaa)
        ctx.fb.color_samples.assign(scene.width * scene.height * 4,
                                    scene.clear_rgba);
    // Allocate depth/stencil buffers up front if any batch / clear op
    // touches them; PFO reads ctx directly so the buffer must already
    // exist before the first relevant fragment lands.
    bool any_depth = false, any_stencil = false;
    for (const auto& op : scene.ops) {
        if (op.kind == SceneOp::BATCH) {
            if (op.batch.depth_test)  any_depth   = true;
            if (op.batch.stencil_test) any_stencil = true;
        }
        if (op.kind == SceneOp::CLEAR_DEPTH)   any_depth   = true;
        if (op.kind == SceneOp::CLEAR_STENCIL) any_stencil = true;
    }
    if (any_depth)
        ctx.fb.depth.assign((size_t)scene.width * scene.height, 1.0f);
    if (any_stencil)
        ctx.fb.stencil.assign((size_t)scene.width * scene.height, 0u);
    rs_pfo.ctx = &ctx;
    auto apply_batch_state = [&](const Batch& b) {
        using DF = gpu::DrawState;
        ctx.draw.depth_test  = b.depth_test;
        ctx.draw.depth_write = b.depth_write;
        ctx.draw.cull_back   = b.cull_back;
        ctx.draw.blend_enable = b.blend;
        if      (b.depth_func == "never")    ctx.draw.depth_func = DF::DF_NEVER;
        else if (b.depth_func == "less")     ctx.draw.depth_func = DF::DF_LESS;
        else if (b.depth_func == "lequal")   ctx.draw.depth_func = DF::DF_LEQUAL;
        else if (b.depth_func == "equal")    ctx.draw.depth_func = DF::DF_EQUAL;
        else if (b.depth_func == "gequal")   ctx.draw.depth_func = DF::DF_GEQUAL;
        else if (b.depth_func == "greater")  ctx.draw.depth_func = DF::DF_GREATER;
        else if (b.depth_func == "notequal") ctx.draw.depth_func = DF::DF_NOTEQUAL;
        else if (b.depth_func == "always")   ctx.draw.depth_func = DF::DF_ALWAYS;
        ctx.draw.stencil_test = b.stencil_test;
        ctx.draw.stencil_ref       = (uint8_t)(b.stencil_ref & 0xFF);
        ctx.draw.stencil_read_mask  = (uint8_t)(b.stencil_read_mask  & 0xFF);
        ctx.draw.stencil_write_mask = (uint8_t)(b.stencil_write_mask & 0xFF);
        if      (b.stencil_func == "never")    ctx.draw.stencil_func = DF::SF_NEVER;
        else if (b.stencil_func == "less")     ctx.draw.stencil_func = DF::SF_LESS;
        else if (b.stencil_func == "lequal")   ctx.draw.stencil_func = DF::SF_LEQUAL;
        else if (b.stencil_func == "greater")  ctx.draw.stencil_func = DF::SF_GREATER;
        else if (b.stencil_func == "gequal")   ctx.draw.stencil_func = DF::SF_GEQUAL;
        else if (b.stencil_func == "equal")    ctx.draw.stencil_func = DF::SF_EQUAL;
        else if (b.stencil_func == "notequal") ctx.draw.stencil_func = DF::SF_NOTEQUAL;
        else if (b.stencil_func == "always")   ctx.draw.stencil_func = DF::SF_ALWAYS;
        auto map_op = [](const std::string& s) {
            if (s == "zero")    return DF::SO_ZERO;
            if (s == "replace") return DF::SO_REPLACE;
            if (s == "incr")    return DF::SO_INCR;
            if (s == "decr")    return DF::SO_DECR;
            if (s == "invert")  return DF::SO_INVERT;
            return DF::SO_KEEP;
        };
        ctx.draw.sop_fail  = map_op(b.stencil_sfail);
        ctx.draw.sop_zfail = map_op(b.stencil_dpfail);
        ctx.draw.sop_zpass = map_op(b.stencil_dppass);
        // Sprint 61 — propagate full blend state to ctx.draw. Without
        // these the SC chain's PFO blends with default SRC_ALPHA /
        // ONE_MINUS_SRC_ALPHA — the 1060 fragment_ops.blend.* cases
        // diverged ~50–150 RMSE from sw_ref because of this exactly.
        auto map_blend_factor = [](const std::string& s) {
            if (s == "zero")                     return DF::BF_ZERO;
            if (s == "one")                      return DF::BF_ONE;
            if (s == "src_color")                return DF::BF_SRC_COLOR;
            if (s == "one_minus_src_color")      return DF::BF_ONE_MINUS_SRC_COLOR;
            if (s == "dst_color")                return DF::BF_DST_COLOR;
            if (s == "one_minus_dst_color")      return DF::BF_ONE_MINUS_DST_COLOR;
            if (s == "src_alpha")                return DF::BF_SRC_ALPHA;
            if (s == "one_minus_src_alpha")      return DF::BF_ONE_MINUS_SRC_ALPHA;
            if (s == "dst_alpha")                return DF::BF_DST_ALPHA;
            if (s == "one_minus_dst_alpha")      return DF::BF_ONE_MINUS_DST_ALPHA;
            if (s == "constant_color")           return DF::BF_CONSTANT_COLOR;
            if (s == "one_minus_constant_color") return DF::BF_ONE_MINUS_CONSTANT_COLOR;
            if (s == "constant_alpha")           return DF::BF_CONSTANT_ALPHA;
            if (s == "one_minus_constant_alpha") return DF::BF_ONE_MINUS_CONSTANT_ALPHA;
            if (s == "src_alpha_saturate")       return DF::BF_SRC_ALPHA_SATURATE;
            return DF::BF_SRC_ALPHA;
        };
        auto map_blend_eq = [](const std::string& s) {
            if (s == "subtract")         return DF::BE_SUBTRACT;
            if (s == "reverse_subtract") return DF::BE_REVERSE_SUBTRACT;
            return DF::BE_ADD;
        };
        ctx.draw.blend_src_rgb   = map_blend_factor(b.blend_src_rgb);
        ctx.draw.blend_dst_rgb   = map_blend_factor(b.blend_dst_rgb);
        ctx.draw.blend_src_alpha = map_blend_factor(b.blend_src_alpha);
        ctx.draw.blend_dst_alpha = map_blend_factor(b.blend_dst_alpha);
        ctx.draw.blend_eq_rgb    = map_blend_eq(b.blend_eq_rgb);
        ctx.draw.blend_eq_alpha  = map_blend_eq(b.blend_eq_alpha);
        ctx.draw.blend_color = {{b.blend_color[0], b.blend_color[1],
                                 b.blend_color[2], b.blend_color[3]}};
        sc_pa.cull_back = b.cull_back;
        // Sprint 61 — per-batch viewport. ScToPaAdapterCa exposes only
        // vp_w / vp_h (the batch-staging knobs); the actual viewport
        // offset lives on `ctx.draw.vp_x / vp_y` and is consumed by
        // primitive_assembly's clip-→screen transform. 0-sized = leave
        // at the scene default (whole-fb).
        if (b.vp_w > 0 && b.vp_h > 0) {
            sc_pa.vp_x = b.vp_x; sc_pa.vp_y = b.vp_y;
            sc_pa.vp_w = b.vp_w; sc_pa.vp_h = b.vp_h;
            ctx.draw.vp_x = b.vp_x; ctx.draw.vp_y = b.vp_y;
            ctx.draw.vp_w = b.vp_w; ctx.draw.vp_h = b.vp_h;
        }
    };

    sc_pa.batch_size = 3;
    sc_pa.vp_w = scene.width; sc_pa.vp_h = scene.height;
    pa_rs.fb_w = scene.width; pa_rs.fb_h = scene.height;
    pa_rs.msaa_4x = scene.msaa;
    pa_rs.varying_count = max_n_vars;
    // Per-quad flush — TBF is flat-cost and RSV is no-op when not MSAA,
    // so each flush is ~30 cycles. Avoids end-of-frame residue (the
    // batched form would leave the trailing < quads_per_flush quads
    // stuck inside PFO_to_TBF). Override via SC_QUADS_PER_FLUSH.
    pfo_tbf.quads_per_flush = 1;
    if (const char* e = std::getenv("SC_QUADS_PER_FLUSH"))
        pfo_tbf.quads_per_flush = std::max(1, std::atoi(e));

    src.clk(clk); src.rst_n(rst_n);
    src.valid(s0_valid); src.ready(s0_ready); src.data(s0_data);

    sc_.clk(clk); sc_.rst_n(rst_n);
    sc_.job_valid_i(s0_valid); sc_.job_ready_o(s0_ready); sc_.job_data_i(s0_data);
    sc_.out_valid_o(s1_valid); sc_.out_ready_i(s1_ready); sc_.out_data_o(s1_data);

    sc_pa.clk(clk); sc_pa.rst_n(rst_n);
    sc_pa.job_valid_i(s1_valid); sc_pa.job_ready_o(s1_ready); sc_pa.job_data_i(s1_data);
    sc_pa.out_valid_o(s2_valid); sc_pa.out_ready_i(s2_ready); sc_pa.out_data_o(s2_data);

    pa.clk(clk); pa.rst_n(rst_n);
    pa.job_valid_i(s2_valid); pa.job_ready_o(s2_ready); pa.job_data_i(s2_data);
    pa.tri_valid_o(s3_valid); pa.tri_ready_i(s3_ready); pa.tri_data_o(s3_data);

    pa_rs.clk(clk); pa_rs.rst_n(rst_n);
    pa_rs.job_valid_i(s3_valid); pa_rs.job_ready_o(s3_ready); pa_rs.job_data_i(s3_data);
    pa_rs.out_valid_o(s4_valid); pa_rs.out_ready_i(s4_ready); pa_rs.out_data_o(s4_data);

    rs.clk(clk); rs.rst_n(rst_n);
    rs.job_valid_i(s4_valid); rs.job_ready_o(s4_ready); rs.job_data_i(s4_data);
    rs.frag_valid_o(s5_valid); rs.frag_ready_i(s5_ready); rs.frag_data_o(s5_data);

    rs_pfo.clk(clk); rs_pfo.rst_n(rst_n);
    rs_pfo.job_valid_i(s5_valid); rs_pfo.job_ready_o(s5_ready); rs_pfo.job_data_i(s5_data);
    rs_pfo.out_valid_o(s6_valid); rs_pfo.out_ready_i(s6_ready); rs_pfo.out_data_o(s6_data);

    pfo.clk(clk); pfo.rst_n(rst_n);
    pfo.job_valid_i(s6_valid); pfo.job_ready_o(s6_ready); pfo.job_data_i(s6_data);
    pfo.out_valid_o(s7_valid); pfo.out_ready_i(s7_ready); pfo.out_data_o(s7_data);

    pfo_tbf.clk(clk); pfo_tbf.rst_n(rst_n);
    pfo_tbf.job_valid_i(s7_valid); pfo_tbf.job_ready_o(s7_ready); pfo_tbf.job_data_i(s7_data);
    pfo_tbf.out_valid_o(s8_valid); pfo_tbf.out_ready_i(s8_ready); pfo_tbf.out_data_o(s8_data);

    tbf.clk(clk); tbf.rst_n(rst_n);
    tbf.job_valid_i(s8_valid); tbf.job_ready_o(s8_ready); tbf.job_data_i(s8_data);
    tbf.out_valid_o(s9_valid); tbf.out_ready_i(s9_ready); tbf.out_data_o(s9_data);

    rsv.clk(clk); rsv.rst_n(rst_n);
    rsv.job_valid_i(s9_valid); rsv.job_ready_o(s9_ready); rsv.job_data_i(s9_data);
    rsv.out_valid_o(s10_valid); rsv.out_ready_i(s10_ready); rsv.out_data_o(s10_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(s10_valid); sink.ready(s10_ready); sink.data(s10_data);

    // ---- Reset, then drive each batch in order through the chain ----
    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    size_t total_verts = 0;
    for (const auto& op : scene.ops)
        if (op.kind == SceneOp::BATCH) total_verts += op.batch.positions.size();

    // ShaderJob storage for the entire scene (kept alive while the chain
    // dereferences pointers to it). Per-batch we reserve a contiguous
    // slice and fill it before pushing.
    std::vector<ShaderJob> jobs(total_verts);

    // Drain timing: idle_threshold must outwait the longest gap between
    // consecutive sink events within a batch. With TBF as a pass-through
    // and RSV no-op for 1× MSAA, the dominant gap is the ShaderCore /
    // PrimitiveAssembly handoff; 50k cycles covers that empirically.
    // MSAA adds O(W×H) per resolve flush on top.
    const sc_core::sc_time step(10, sc_core::SC_US);   // 1000 cycles
    const int max_steps = 200000;                       // 2 s sim cap
    const long max_flush_cycles =
        scene.msaa ? (long)scene.width * scene.height + 64L : 64L;
    const int idle_threshold =
        static_cast<int>(std::max(50L, (max_flush_cycles * 2) / 1000 + 50));

    // Drain the chain after a batch push: spin until sink stops growing
    // for `idle_threshold` polling steps. Returns early without simulating
    // anything when nothing is in flight (no pushes since last drain),
    // so back-to-back CLEAR ops or a CLEAR before the first batch don't
    // burn 2s of wall-clock per op spinning the chain dry.
    auto drain_pipeline = [&](int& seen_at_start, bool any_pushed) {
        if (!any_pushed) return;
        int prev_seen = -1;
        int idle_steps = 0;
        for (int i = 0; i < max_steps; ++i) {
            sc_core::sc_start(step);
            if (sink.seen > seen_at_start && sink.seen == prev_seen) {
                if (++idle_steps >= idle_threshold) break;
            } else {
                idle_steps = 0;
            }
            prev_seen = sink.seen;
        }
        seen_at_start = sink.seen;
    };

    size_t job_off = 0;
    int    seen_at_start = 0;
    bool   pending_drain = false;
    auto blit_bitmap = [&](const BitmapOp& bm) {
        const int FB_W = scene.width, FB_H = scene.height;
        const int row_bytes = (bm.w + 7) / 8;
        for (int row = 0; row < bm.h; ++row) {
            const uint8_t* src = bm.bits.data()
                               + (size_t)(bm.h - 1 - row) * row_bytes;
            const int fy = bm.y - row;
            if (fy < 0 || fy >= FB_H) continue;
            for (int col = 0; col < bm.w; ++col) {
                if (!(src[col >> 3] & (0x80 >> (col & 7)))) continue;
                const int fx = bm.x + col;
                if (fx < 0 || fx >= FB_W) continue;
                ctx.fb.color[(size_t)fy * FB_W + fx] = bm.color;
            }
        }
    };

    for (const auto& op : scene.ops) {
        if (op.kind == SceneOp::CLEAR) {
            // Drain in-flight fragments first so this clear lands on a
            // quiescent fb (otherwise stragglers from the previous
            // batch would write on top of the cleared pixels).
            drain_pipeline(seen_at_start, pending_drain);
            pending_drain = false;
            // Sprint 60 — honour per-CLEAR scissor rect + color mask
            // lane. `clear_rect_full=true` keeps the legacy fast-path:
            // whole-fb fill with the clear color. Otherwise the loop
            // applies `pix = (old & ~lane) | (rgba & lane)` only inside
            // [x0,x1)×[y0,y1). Without this, scissored / masked
            // color_clear cases diverged ~100 RMSE from sw_ref.
            const int W = ctx.fb.width;
            if (op.clear_rect_full && op.clear_lane == 0xFFFFFFFFu) {
                std::fill(ctx.fb.color.begin(), ctx.fb.color.end(), op.clear_rgba);
            } else {
                const uint32_t lane = op.clear_lane;
                const uint32_t pix  = op.clear_rgba;
                int x0 = op.clear_x0, y0 = op.clear_y0;
                int x1 = op.clear_x1, y1 = op.clear_y1;
                if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
                if (x1 > W) x1 = W; if (y1 > ctx.fb.height) y1 = ctx.fb.height;
                for (int y = y0; y < y1; ++y) {
                    uint32_t* row = &ctx.fb.color[(size_t)y * W];
                    for (int x = x0; x < x1; ++x)
                        row[x] = (row[x] & ~lane) | (pix & lane);
                }
            }
            if (!ctx.fb.depth.empty())
                std::fill(ctx.fb.depth.begin(), ctx.fb.depth.end(), 1.0f);
            continue;
        }
        if (op.kind == SceneOp::BITMAP) {
            drain_pipeline(seen_at_start, pending_drain);
            pending_drain = false;
            blit_bitmap(op.bitmap);
            continue;
        }
        if (op.kind == SceneOp::CLEAR_DEPTH) {
            drain_pipeline(seen_at_start, pending_drain);
            pending_drain = false;
            if (!ctx.fb.depth.empty())
                std::fill(ctx.fb.depth.begin(), ctx.fb.depth.end(), op.clear_depth);
            continue;
        }
        if (op.kind == SceneOp::CLEAR_STENCIL) {
            drain_pipeline(seen_at_start, pending_drain);
            pending_drain = false;
            if (!ctx.fb.stencil.empty())
                std::fill(ctx.fb.stencil.begin(), ctx.fb.stencil.end(),
                          (uint8_t)(op.clear_stencil_val & 0xFF));
            continue;
        }
        const Batch& batch = op.batch;
        apply_batch_state(batch);
        const size_t n = batch.positions.size();
        const int    nv = std::max(1, std::min(7, batch.n_vars));
        for (size_t i = 0; i < n; ++i) {
            ShaderJob& j = jobs[job_off + i];
            j.code = &code;
            j.is_vs = true;
            // Sprint 61 — c0 = pos, c1..cN = varyings.
            for (int k = 0; k < 4; ++k) {
                j.constants[0][k] = batch.positions[i][k];
                for (int v = 0; v < nv; ++v)
                    j.constants[1 + v][k] = batch.varyings[i][v][k];
            }
            src.push(reinterpret_cast<uint64_t>(&j));
        }
        job_off += n;
        pending_drain = (n > 0);
        drain_pipeline(seen_at_start, pending_drain);
        pending_drain = false;
    }

    // ---- Dump PPM + cycle metric ----
    if (!write_ppm(out_ppm, ctx.fb.color, scene.width, scene.height)) {
        std::fprintf(stderr, "FAIL: cannot write %s\n", out_ppm.c_str());
        return 1;
    }
    const double ns       = sc_core::sc_time_stamp().to_double();   // ps default
    const double clock_ps = sc_core::sc_time(10, sc_core::SC_NS).to_double();
    const long   cycles   = static_cast<long>(ns / clock_ps);
    int painted = 0;
    for (uint32_t c : ctx.fb.color)
        if (c != scene.clear_rgba) ++painted;
    std::printf("PPM=%s\n", out_ppm.c_str());
    std::printf("CYCLES=%ld\n", cycles);
    std::printf("FLUSHES=%d\n", sink.seen);
    std::printf("PAINTED=%d\n", painted);
    std::printf("TRIANGLES=%zu\n", total_verts / 3);

    // Optional bit-parity check vs a reference PPM. The conformance
    // ctests pass the matching tests/scenes/<name>.golden.ppm here so
    // any drift between the SC chain and sw_ref shows up as a hard fail.
    if (!ref_ppm.empty()) {
        std::vector<uint32_t> ref;
        int rW = 0, rH = 0;
        std::string rerr;
        if (!read_ppm(ref_ppm, ref, rW, rH, rerr)) {
            std::fprintf(stderr, "FAIL: ref read: %s\n", rerr.c_str());
            return 1;
        }
        if (rW != scene.width || rH != scene.height) {
            std::fprintf(stderr, "FAIL: ref size %dx%d != scene %dx%d\n",
                         rW, rH, scene.width, scene.height);
            return 1;
        }
        const float r = rmse(ctx.fb.color, ref);
        std::printf("RMSE=%g\n", r);
        if (rmse_max >= 0.0f && r > rmse_max) {
            std::fprintf(stderr, "FAIL: rmse %g exceeds %g\n", r, rmse_max);
            return 1;
        }
    }
    return 0;
}
