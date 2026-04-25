// Sprint 25 — cycle-accurate PerFragmentOps testbench.
//
// 4×4 framebuffer, depth_test off / depth_write off, no blend.
// Push one PfoJob with a Quad of 4 fragments at distinct pixels with
// distinct colours. Assert each pixel landed at the expected RGBA8.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/perfragmentops_ca.h"

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
    sc_core::sc_signal<bool>    j_valid, j_ready, o_valid, o_ready;
    sc_core::sc_signal<uint64_t> j_data, o_data;

    Source           src ("src");
    PerFragmentOpsCa pfo ("pfo");
    Sink             sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    pfo.clk(clk); pfo.rst_n(rst_n);
    pfo.job_valid_i(j_valid); pfo.job_ready_o(j_ready); pfo.job_data_i(j_data);
    pfo.out_valid_o(o_valid); pfo.out_ready_i(o_ready); pfo.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // 4x4 fb, no MSAA, no depth/stencil/blend.
    gpu::Context ctx;
    ctx.fb.width = 4; ctx.fb.height = 4; ctx.fb.msaa_4x = false;
    ctx.fb.color.assign(16, 0u);
    ctx.draw.depth_test = false;
    ctx.draw.depth_write = false;
    ctx.draw.stencil_test = false;
    ctx.draw.blend_enable = false;

    gpu::Quad quad;
    auto set_frag = [&](int i, int x, int y, float r, float g, float b) {
        auto& f = quad.frags[i];
        f.pos = {x, y};
        f.coverage_mask = 0x1;
        f.depth = 0.0f;
        f.varying[0] = {{r, g, b, 1.0f}};
        f.varying_count = 1;
    };
    set_frag(0, 0, 0, 1.0f, 0.0f, 0.0f);   // red
    set_frag(1, 1, 0, 0.0f, 1.0f, 0.0f);   // green
    set_frag(2, 0, 1, 0.0f, 0.0f, 1.0f);   // blue
    set_frag(3, 1, 1, 1.0f, 1.0f, 1.0f);   // white

    PfoJob job;
    job.ctx  = &ctx;
    job.quad = &quad;

    src.push(reinterpret_cast<uint64_t>(&job));
    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    auto px = [&](int x, int y) { return ctx.fb.color[y * 4 + x]; };
    // pack_rgba8: (A<<24)|(B<<16)|(G<<8)|R
    if ((px(0,0) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (0,0) red byte 0x%x\n", px(0,0)); return 1;
    }
    if (((px(1,0) >> 8) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (1,0) green byte 0x%x\n", px(1,0)); return 1;
    }
    if (((px(0,1) >> 16) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (0,1) blue byte 0x%x\n", px(0,1)); return 1;
    }
    if (px(1,1) != 0xFFFFFFFFu) {
        std::fprintf(stderr, "FAIL: (1,1) white pixel 0x%x\n", px(1,1)); return 1;
    }
    std::printf("PASS — PFO_ca wrote 4 fragments @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
