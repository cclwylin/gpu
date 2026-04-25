// Sprint 23 — chain testbench: CP_ca -> VF_ca -> SC_ca.
//
// One ShaderJob is enqueued at CP. VF fans out vertices_per_cmd (3)
// vertex jobs. SC runs ISA on each. Sink sees the same job pointer 3
// times and the job's o0 ends up at c0+c1.

#include <cmath>
#include <cstdio>
#include <systemc>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/gpu_top_ca.h"

using namespace gpu::systemc;

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
    sc_core::sc_signal<bool>    o_valid, o_ready;
    sc_core::sc_signal<uint64_t> o_data;

    GpuTopCa top("top");
    Sink     sink("sink");

    top.clk(clk); top.rst_n(rst_n);
    top.out_valid_o(o_valid); top.out_ready_i(o_ready); top.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // `add o0, c0, c1`
    auto a = gpu::asm_::assemble("add o0, c0, c1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str());
        return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    ShaderJob job{};
    job.code = &code;
    job.is_vs = true;
    job.constants[0] = {{1.0f, 2.0f, 3.0f, 4.0f}};
    job.constants[1] = {{0.5f, 0.5f, 0.5f, 0.5f}};

    top.cp.enqueue(&job);

    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 3u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 3)\n", sink.seen.size());
        return 1;
    }
    for (size_t i = 0; i < sink.seen.size(); ++i) {
        if (sink.seen[i] != reinterpret_cast<uint64_t>(&job)) {
            std::fprintf(stderr, "FAIL: sink[%zu] pointer mismatch\n", i);
            return 1;
        }
    }
    if (std::fabs(job.outputs[0][0] - 1.5f) > 1e-5f) {
        std::fprintf(stderr, "FAIL: o0.x = %g (expected 1.5)\n",
                     (double)job.outputs[0][0]);
        return 1;
    }
    std::printf("PASS — chain CP->VF->SC delivered 3 jobs @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
