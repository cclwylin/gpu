// Sprint 22 — cycle-accurate Rasterizer testbench.
//
// Builds a RasterJob with one screen-space triangle (1× sample, 32×32 fb),
// pushes through RS_ca, asserts:
//   * sink saw the job pointer back
//   * fragments.size() falls in a sensible range (~ 200-300 for the
//     half-screen triangle below)
//   * at least one fragment has coverage_mask == 0x1

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/rasterizer_ca.h"

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
    sc_core::sc_signal<bool>    j_valid, j_ready, f_valid, f_ready;
    sc_core::sc_signal<uint64_t> j_data, f_data;

    Source       src("src");
    RasterizerCa rs ("rs");
    Sink         sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    rs.clk(clk); rs.rst_n(rst_n);
    rs.job_valid_i(j_valid); rs.job_ready_o(j_ready); rs.job_data_i(j_data);
    rs.frag_valid_o(f_valid); rs.frag_ready_i(f_ready); rs.frag_data_o(f_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(f_valid); sink.ready(f_ready); sink.data(f_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // Build one screen-space triangle covering ~half the 32×32 framebuffer.
    RasterJob job{};
    job.fb_w = 32; job.fb_h = 32; job.msaa_4x = false; job.varying_count = 0;
    job.triangles.resize(1);
    auto& tri = job.triangles[0];
    // pos: vec4{x, y, depth, 1/w}; varying slots zeroed.
    tri[0][0] = {{16.0f,  4.0f, 0.5f, 1.0f}};
    tri[1][0] = {{ 4.0f, 28.0f, 0.5f, 1.0f}};
    tri[2][0] = {{28.0f, 28.0f, 0.5f, 1.0f}};

    src.push(reinterpret_cast<uint64_t>(&job));
    sc_core::sc_start(50, sc_core::SC_US);

    if (sink.seen.size() != 1) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    const size_t n = job.fragments.size();
    std::printf("RS_ca emitted %zu fragments\n", n);
    if (n < 100 || n > 600) {
        std::fprintf(stderr, "FAIL: fragment count %zu outside [100,600]\n", n);
        return 1;
    }
    if ((job.fragments[0].coverage_mask & 0x1) == 0) {
        std::fprintf(stderr, "FAIL: first fragment has zero coverage\n");
        return 1;
    }
    std::printf("PASS @ %s\n", sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
