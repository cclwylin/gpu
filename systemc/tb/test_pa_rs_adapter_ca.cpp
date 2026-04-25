// Sprint 30 — PA_ca → PA_to_RS_adapter → RS_ca chain testbench.
//
// Build a clip-space triangle PrimAssemblyJob (3 vs_outputs), push
// through PA_ca, adapter repackages into a RasterJob with fb=32×32,
// RS_ca rasterises. Assert: sink saw 1 RasterJob*, fragment count
// in [100, 600] (same range as the standalone Sprint-22 RS_ca test).

#include <array>
#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/pa_to_rs_adapter_ca.h"
#include "gpu_systemc/primitiveassembly_ca.h"
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
    sc_core::sc_signal<bool>    a_valid, a_ready, b_valid, b_ready, c_valid, c_ready, o_valid, o_ready;
    sc_core::sc_signal<uint64_t> a_data, b_data, c_data, o_data;

    Source              src ("src");
    PrimitiveAssemblyCa pa  ("pa");
    PaToRsAdapterCa     adp ("adp");
    RasterizerCa        rs  ("rs");
    Sink                sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(a_valid); src.ready(a_ready); src.data(a_data);

    pa.clk(clk); pa.rst_n(rst_n);
    pa.job_valid_i(a_valid); pa.job_ready_o(a_ready); pa.job_data_i(a_data);
    pa.tri_valid_o(b_valid); pa.tri_ready_i(b_ready); pa.tri_data_o(b_data);

    adp.clk(clk); adp.rst_n(rst_n);
    adp.job_valid_i(b_valid); adp.job_ready_o(b_ready); adp.job_data_i(b_data);
    adp.out_valid_o(c_valid); adp.out_ready_i(c_ready); adp.out_data_o(c_data);

    rs.clk(clk); rs.rst_n(rst_n);
    rs.job_valid_i(c_valid); rs.job_ready_o(c_ready); rs.job_data_i(c_data);
    rs.frag_valid_o(o_valid); rs.frag_ready_i(o_ready); rs.frag_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    PrimAssemblyJob pa_job{};
    pa_job.vp_w = 32; pa_job.vp_h = 32; pa_job.cull_back = false;
    pa_job.vs_outputs.resize(3);
    pa_job.vs_outputs[0][0] = {{ 0.0f,  0.7f, 0.0f, 1.0f}};
    pa_job.vs_outputs[1][0] = {{-0.7f, -0.7f, 0.0f, 1.0f}};
    pa_job.vs_outputs[2][0] = {{ 0.7f, -0.7f, 0.0f, 1.0f}};

    src.push(reinterpret_cast<uint64_t>(&pa_job));
    sc_core::sc_start(50, sc_core::SC_US);

    if (sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    const auto* rj = reinterpret_cast<const RasterJob*>(sink.seen[0]);
    if (rj != &adp.staged()) {
        std::fprintf(stderr, "FAIL: sink ptr != adapter.staged\n");
        return 1;
    }
    const size_t n = adp.staged().fragments.size();
    if (n < 100 || n > 600) {
        std::fprintf(stderr, "FAIL: fragments = %zu (expected 100..600)\n", n);
        return 1;
    }
    std::printf("PASS — PA→adapter→RS chain emitted %zu fragments @ %s\n",
                n, sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
