// Sprint 21 — cycle-accurate PrimitiveAssembly testbench.
//
// Builds a PrimAssemblyJob with 3 vs_outputs (one triangle in clip-space),
// pushes through PA_ca, asserts 1 triangle out + screen-space y of the
// top vertex matches the LT block's behaviour (≈ 27.2 on a 32×32 vp).

#include <array>
#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/primitiveassembly_ca.h"

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
    sc_core::sc_signal<bool>    j_valid, j_ready, t_valid, t_ready;
    sc_core::sc_signal<uint64_t> j_data, t_data;

    Source              src("src");
    PrimitiveAssemblyCa pa ("pa");
    Sink                sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    pa.clk(clk); pa.rst_n(rst_n);
    pa.job_valid_i(j_valid); pa.job_ready_o(j_ready); pa.job_data_i(j_data);
    pa.tri_valid_o(t_valid); pa.tri_ready_i(t_ready); pa.tri_data_o(t_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(t_valid); sink.ready(t_ready); sink.data(t_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    PrimAssemblyJob job{};
    job.vp_w = 32; job.vp_h = 32; job.cull_back = false;
    job.vs_outputs.resize(3);
    job.vs_outputs[0][0] = {{ 0.0f,  0.7f, 0.0f, 1.0f}};
    job.vs_outputs[1][0] = {{-0.7f, -0.7f, 0.0f, 1.0f}};
    job.vs_outputs[2][0] = {{ 0.7f, -0.7f, 0.0f, 1.0f}};

    src.push(reinterpret_cast<uint64_t>(&job));
    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 1) {
        std::fprintf(stderr, "FAIL: sink saw %zu jobs (expected 1)\n", sink.seen.size());
        return 1;
    }
    if (job.triangles.size() != 1) {
        std::fprintf(stderr, "FAIL: triangles = %zu (expected 1)\n", job.triangles.size());
        return 1;
    }
    const float sy0 = job.triangles[0][0][0][1];
    if (sy0 < 26.0f || sy0 > 28.0f) {
        std::fprintf(stderr, "FAIL: top-vertex screen y = %g (expected ~27.2)\n", sy0);
        return 1;
    }
    std::printf("PASS — 1 triangle out, top y=%g @ %s\n",
                sy0, sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
