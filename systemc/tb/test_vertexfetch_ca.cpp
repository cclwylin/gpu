// Sprint 19 — cycle-accurate VertexFetch testbench.
//
// Drives N=3 upstream commands; VF expands each to vertices_per_cmd=3
// downstream vertex jobs; sink consumes and records. Asserts:
//   * sink saw exactly 3 * 3 = 9 jobs
//   * each job's data matches the originating upstream cmd

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/vertexfetch_ca.h"

using namespace gpu::systemc;

// Source: drives a queue of cmds onto cmd_valid/data, honours ready.
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
        valid.write(false);
        data.write(0);
        while (true) {
            if (q.empty()) { valid.write(false); wait(); continue; }
            valid.write(true);
            data.write(q.front());
            wait();
            while (!ready.read()) wait();
            // handshake completes this cycle (ready & valid both high).
            q.pop();
            valid.write(false);
            wait();
        }
    }
};

// Sink: always asserts ready, records each accepted data word.
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
    sc_core::sc_signal<bool>    cmd_valid;
    sc_core::sc_signal<bool>    cmd_ready;
    sc_core::sc_signal<uint64_t> cmd_data;
    sc_core::sc_signal<bool>    vert_valid;
    sc_core::sc_signal<bool>    vert_ready;
    sc_core::sc_signal<uint64_t> vert_data;

    Source        src("src");
    VertexFetchCa vf("vf");
    Sink          sink("sink");

    src.clk(clk);   src.rst_n(rst_n);
    src.valid(cmd_valid); src.ready(cmd_ready); src.data(cmd_data);

    vf.clk(clk);    vf.rst_n(rst_n);
    vf.cmd_valid_i(cmd_valid);  vf.cmd_ready_o(cmd_ready);  vf.cmd_data_i(cmd_data);
    vf.vert_valid_o(vert_valid); vf.vert_ready_i(vert_ready); vf.vert_data_o(vert_data);

    sink.clk(clk);  sink.rst_n(rst_n);
    sink.valid(vert_valid); sink.ready(vert_ready); sink.data(vert_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    src.push(0xCAFEBABE00000001ull);
    src.push(0xCAFEBABE00000002ull);
    src.push(0xCAFEBABE00000003ull);

    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 9) {
        std::fprintf(stderr, "FAIL: sink saw %zu jobs (expected 9)\n", sink.seen.size());
        return 1;
    }
    for (size_t i = 0; i < 9; ++i) {
        uint64_t expected = 0xCAFEBABE00000001ull + (i / 3);
        if (sink.seen[i] != expected) {
            std::fprintf(stderr, "FAIL: job[%zu] = 0x%llx, expected 0x%llx\n",
                         i, (unsigned long long)sink.seen[i],
                         (unsigned long long)expected);
            return 1;
        }
    }
    std::printf("PASS — %zu vertex jobs from 3 cmds @ %s\n",
                sink.seen.size(), sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
