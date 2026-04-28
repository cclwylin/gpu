// Sprint 20 — cycle-accurate ShaderCore testbench.
//
// Assembles a tiny `add o0, c0, c1` shader once, builds 3 ShaderJob
// instances (each with different constants), feeds them through SC_ca,
// asserts each job's o0 lands at the expected (c0 + c1) value.

#include <array>
#include <cmath>
#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/shadercore_ca.h"

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
        SC_CTHREAD(thread, clk.pos());
        reset_signal_is(rst_n, false);
    }
    void thread() {
        valid.write(false); data.write(0);
        while (true) {
            if (q.empty()) { valid.write(false); wait(); continue; }
            valid.write(true); data.write(q.front());
            wait();
            while (!ready.read()) wait();
            q.pop();
            valid.write(false);
            wait();
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
        SC_CTHREAD(thread, clk.pos());
        reset_signal_is(rst_n, false);
    }
    void thread() {
        ready.write(false);
        while (true) {
            ready.write(true);
            wait();
            if (valid.read()) seen.push_back(data.read());
        }
    }
};

int sc_main(int /*argc*/, char** /*argv*/) {
    sc_core::sc_clock           clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool>    rst_n;
    sc_core::sc_signal<bool>    j_valid;
    sc_core::sc_signal<bool>    j_ready;
    sc_core::sc_signal<uint64_t> j_data;
    sc_core::sc_signal<bool>    o_valid;
    sc_core::sc_signal<bool>    o_ready;
    sc_core::sc_signal<uint64_t> o_data;

    Source       src ("src");
    ShaderCoreCa sc_ca("sc_ca");
    Sink         sink("sink");

    src.clk(clk);   src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    sc_ca.clk(clk); sc_ca.rst_n(rst_n);
    sc_ca.job_valid_i(j_valid); sc_ca.job_ready_o(j_ready); sc_ca.job_data_i(j_data);
    sc_ca.out_valid_o(o_valid); sc_ca.out_ready_i(o_ready); sc_ca.out_data_o(o_data);

    sink.clk(clk);  sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // One shared shader binary — `add o0, c0, c1`.
    auto a = gpu::asm_::assemble("add o0, c0, c1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str());
        return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    // Three jobs with different constants.
    std::vector<ShaderJob> jobs(3);
    for (int i = 0; i < 3; ++i) {
        jobs[i].code = &code;
        jobs[i].is_vs = true;
        jobs[i].constants[0] = {{1.0f + i, 2.0f + i, 3.0f + i, 4.0f + i}};
        jobs[i].constants[1] = {{0.5f, 0.5f, 0.5f, 0.5f}};
        src.push(reinterpret_cast<uint64_t>(&jobs[i]));
    }

    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 3) {
        std::fprintf(stderr, "FAIL: sink saw %zu jobs (expected 3)\n", sink.seen.size());
        return 1;
    }
    for (int i = 0; i < 3; ++i) {
        const auto& o = jobs[i].outputs[0];
        const float expected_x = 1.0f + i + 0.5f;
        if (std::fabs(o[0] - expected_x) > 1e-5f) {
            std::fprintf(stderr, "FAIL: job %d out.x = %g vs expected %g\n",
                         i, (double)o[0], (double)expected_x);
            return 1;
        }
    }
    std::printf("PASS — 3 jobs executed by SC_ca @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
