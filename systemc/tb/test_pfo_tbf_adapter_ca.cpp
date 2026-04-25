// Sprint 32 — PFO_to_TBF_adapter standalone testbench.
//
// Push 4 PfoJobs (each carrying the same Context*) into the adapter
// configured with quads_per_flush=2. Expect 2 TileFlushJobs at the
// sink, each whose ctx == the input ctx and tile dims == fb dims.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/pfo_to_tbf_adapter_ca.h"

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

    Source             src ("src");
    PfoToTbfAdapterCa  adp ("adp");
    Sink               sink("sink");

    adp.quads_per_flush = 2;

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    adp.clk(clk); adp.rst_n(rst_n);
    adp.job_valid_i(j_valid); adp.job_ready_o(j_ready); adp.job_data_i(j_data);
    adp.out_valid_o(o_valid); adp.out_ready_i(o_ready); adp.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    gpu::Context ctx;
    ctx.fb.width = 16; ctx.fb.height = 16;

    std::vector<PfoJob> jobs(4);
    for (auto& j : jobs) { j.ctx = &ctx; j.quad = nullptr; }
    for (auto& j : jobs) src.push(reinterpret_cast<uint64_t>(&j));

    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 2u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 2)\n", sink.seen.size());
        return 1;
    }
    const auto* tj = reinterpret_cast<const TileFlushJob*>(sink.seen[0]);
    if (tj->ctx != &ctx) {
        std::fprintf(stderr, "FAIL: TileFlushJob.ctx mismatch\n");
        return 1;
    }
    if (tj->tile_w != 16 || tj->tile_h != 16) {
        std::fprintf(stderr, "FAIL: tile dims %d×%d (expected 16×16)\n",
                     tj->tile_w, tj->tile_h);
        return 1;
    }
    std::printf("PASS — PFO→TBF adapter: 4 quads → 2 flushes @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
