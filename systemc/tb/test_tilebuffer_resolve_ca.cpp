// Sprint 26 — TBF_ca + RSV_ca chained testbench.
//
// 4×4 MSAA-4× framebuffer; samples per pixel pre-loaded with 4 known
// values whose box average gives a deterministic RGBA8. Push one
// TileFlushJob through TBF_ca → RSV_ca, assert each fb.color pixel
// equals the expected average.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/resolveunit_ca.h"
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
    sc_core::sc_signal<bool>    j_valid, j_ready, m_valid, m_ready, o_valid, o_ready;
    sc_core::sc_signal<uint64_t> j_data, m_data, o_data;

    Source         src ("src");
    TileBufferCa   tbf ("tbf");
    ResolveUnitCa  rsv ("rsv");
    Sink           sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    tbf.clk(clk); tbf.rst_n(rst_n);
    tbf.job_valid_i(j_valid); tbf.job_ready_o(j_ready); tbf.job_data_i(j_data);
    tbf.out_valid_o(m_valid); tbf.out_ready_i(m_ready); tbf.out_data_o(m_data);

    rsv.clk(clk); rsv.rst_n(rst_n);
    rsv.job_valid_i(m_valid); rsv.job_ready_o(m_ready); rsv.job_data_i(m_data);
    rsv.out_valid_o(o_valid); rsv.out_ready_i(o_ready); rsv.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // 4×4 fb, MSAA-4×; preload color_samples so box-avg is predictable.
    gpu::Context ctx;
    ctx.fb.width = 4; ctx.fb.height = 4; ctx.fb.msaa_4x = true;
    ctx.fb.color.assign(16, 0u);
    ctx.fb.color_samples.assign(16 * 4, 0u);
    // For every pixel, set s0=0xFF (R only), s1=0xFF (R only), s2=0, s3=0.
    // Box avg: (0xFF + 0xFF + 0 + 0 + 2) >> 2 = 0x80 in R, others 0.
    for (size_t pix = 0; pix < 16; ++pix) {
        ctx.fb.color_samples[pix * 4 + 0] = 0x000000FFu;
        ctx.fb.color_samples[pix * 4 + 1] = 0x000000FFu;
        ctx.fb.color_samples[pix * 4 + 2] = 0u;
        ctx.fb.color_samples[pix * 4 + 3] = 0u;
    }

    TileFlushJob job;
    job.ctx = &ctx;
    job.tile_x = 0; job.tile_y = 0; job.tile_w = 4; job.tile_h = 4;

    src.push(reinterpret_cast<uint64_t>(&job));
    sc_core::sc_start(5, sc_core::SC_US);

    if (sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    for (size_t pix = 0; pix < 16; ++pix) {
        const uint32_t got = ctx.fb.color[pix];
        const uint32_t expected = 0x00000080u;   // R=0x80, G=B=A=0
        if (got != expected) {
            std::fprintf(stderr, "FAIL: pix %zu = 0x%x (expected 0x%x)\n",
                         pix, got, expected);
            return 1;
        }
    }
    std::printf("PASS — TBF→RSV resolved 16 px @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
