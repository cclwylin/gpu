// Sprint 33 — Phase 2.x full-chain end-to-end testbench.
//
// Wires the pipeline from SC_ca through to RSV_ca with all four
// payload-conversion adapters in place:
//
//   src → SC_ca → SC_to_PA → PA_ca → PA_to_RS → RS_ca →
//         RS_to_PFO → PFO_ca → PFO_to_TBF → TBF_ca → RSV_ca → sink
//
// Three ShaderJobs run a 2-instruction VS that copies c0 → o0
// (position) and c1 → o1 (varying[0] = colour). With c1 = white at
// every vertex, RS interpolates a constant-white varying[0]. PFO
// packs (1,1,1,1) into 0xFFFFFFFF and writes it to fb.color.
//
// Asserts:
//   * sink saw ≥ 1 TileFlushJob*
//   * ctx.fb.color contains at least one 0xFFFFFFFF pixel (proving
//     fragments flowed end-to-end through every adapter and block)
//   * the painted-pixel count is ≥ 100 (matches the standalone
//     RS_ca fragment-count range from Sprint 22)

#include <array>
#include <cstdio>
#include <queue>
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

SC_MODULE(Source) {
    sc_core::sc_in<bool>     clk;
    sc_core::sc_in<bool>     rst_n;
    sc_core::sc_out<bool>    valid;
    sc_core::sc_in<bool>     ready;
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
    sc_core::sc_in<bool>     clk;
    sc_core::sc_in<bool>     rst_n;
    sc_core::sc_in<bool>     valid;
    sc_core::sc_out<bool>    ready;
    sc_core::sc_in<uint64_t> data;
    std::vector<uint64_t> seen;
    SC_HAS_PROCESS(Sink);
    explicit Sink(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        ready.write(false);
        while (true) { ready.write(true); wait();
            if (valid.read()) seen.push_back(data.read()); }
    }
};

int sc_main(int /*argc*/, char** /*argv*/) {
    sc_core::sc_clock           clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool>    rst_n;

    // 11 inter-block signals (one valid/ready/data triple per hop).
    #define DECL_HOP(name) \
        sc_core::sc_signal<bool>     name##_valid, name##_ready; \
        sc_core::sc_signal<uint64_t> name##_data
    DECL_HOP(s0); DECL_HOP(s1); DECL_HOP(s2); DECL_HOP(s3); DECL_HOP(s4);
    DECL_HOP(s5); DECL_HOP(s6); DECL_HOP(s7); DECL_HOP(s8); DECL_HOP(s9);
    DECL_HOP(s10);
    #undef DECL_HOP

    Source            src    ("src");
    ShaderCoreCa      sc_    ("sc");
    ScToPaAdapterCa   sc_pa  ("sc_pa");
    PrimitiveAssemblyCa pa   ("pa");
    PaToRsAdapterCa   pa_rs  ("pa_rs");
    RasterizerCa      rs     ("rs");
    RsToPfoAdapterCa  rs_pfo ("rs_pfo");
    PerFragmentOpsCa  pfo    ("pfo");
    PfoToTbfAdapterCa pfo_tbf("pfo_tbf");
    TileBufferCa      tbf    ("tbf");
    ResolveUnitCa     rsv    ("rsv");
    Sink              sink   ("sink");

    // Framebuffer + draw state shared by RS_to_PFO and PFO_to_TBF.
    gpu::Context ctx;
    ctx.fb.width = 32; ctx.fb.height = 32; ctx.fb.msaa_4x = false;
    ctx.fb.color.assign(32 * 32, 0u);
    ctx.draw.depth_test = false;
    ctx.draw.depth_write = false;
    ctx.draw.stencil_test = false;
    ctx.draw.blend_enable = false;
    rs_pfo.ctx = &ctx;

    // Adapter knobs.
    sc_pa.batch_size = 3;
    sc_pa.vp_w = 32; sc_pa.vp_h = 32;
    pa_rs.fb_w = 32; pa_rs.fb_h = 32;
    pa_rs.varying_count = 1;
    pfo_tbf.quads_per_flush = 1;     // per-quad flush

    // Wiring.
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

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // VS: copy c0 → o0 (position), c1 → o1 (varying[0] colour).
    auto a = gpu::asm_::assemble("mov o0, c0\nmov o1, c1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str()); return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    const std::array<gpu::sim::Vec4, 3> verts = {{
        {{16.0f,  4.0f, 0.5f, 1.0f}},
        {{ 4.0f, 28.0f, 0.5f, 1.0f}},
        {{28.0f, 28.0f, 0.5f, 1.0f}},
    }};
    // PA does a perspective divide + viewport transform; to land at
    // the same screen-space coords we used in the RS_ca standalone
    // test, feed clip-space verts that *map back* to those screen
    // coordinates. With viewport = full fb (32×32, origin (0,0)) and
    // ndc = clip / w with w = 1, the inverse map is:
    //   ndc.x = 2 * (sx / vp_w) - 1
    //   ndc.y = 2 * (sy / vp_h) - 1
    auto to_clip = [](float sx, float sy) -> gpu::sim::Vec4 {
        const float ndc_x = 2.0f * (sx / 32.0f) - 1.0f;
        const float ndc_y = 2.0f * (sy / 32.0f) - 1.0f;
        return {{ndc_x, ndc_y, 0.0f, 1.0f}};
    };

    std::vector<ShaderJob> jobs(3);
    for (int i = 0; i < 3; ++i) {
        jobs[i].code = &code;
        jobs[i].is_vs = true;
        jobs[i].constants[0] = to_clip(verts[i][0], verts[i][1]);
        jobs[i].constants[1] = {{1.0f, 1.0f, 1.0f, 1.0f}};   // white
        src.push(reinterpret_cast<uint64_t>(&jobs[i]));
    }

    sc_core::sc_start(2, sc_core::SC_MS);

    // Count pixels painted white.
    size_t white = 0;
    for (const uint32_t px : ctx.fb.color) if (px == 0xFFFFFFFFu) ++white;

    if (sink.seen.empty()) {
        std::fprintf(stderr, "FAIL: sink saw 0 TileFlushJobs\n"); return 1;
    }
    if (white < 100) {
        std::fprintf(stderr,
                     "FAIL: only %zu white pixels (expected ≥ 100)\n", white);
        return 1;
    }
    std::printf("PASS — full chain painted %zu white pixels, %zu flushes @ %s\n",
                white, sink.seen.size(),
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
