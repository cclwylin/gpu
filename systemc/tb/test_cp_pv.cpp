// Sprint 18 — Phase 2 kickoff: cycle-accurate CP smoke.
//
// Drives the new CommandProcessorPv with a simple valid/ready receiver,
// asserts each enqueued job appears on the cmd_data_o port within a
// bounded number of cycles, then ready handshakes to consume.

#include <cstdio>
#include <queue>
#include <systemc>

#include "gpu_systemc/commandprocessor_pv.h"

using namespace gpu::systemc;

// Tiny consumer that always asserts ready and records the data words it sees.
SC_MODULE(Sink) {
    sc_core::sc_in<bool>  clk;
    sc_core::sc_in<bool>  rst_n;
    sc_core::sc_in<bool>  valid;
    sc_core::sc_out<bool> ready;
    sc_core::sc_in<uint64_t> data;
    std::vector<uint64_t> seen;

    SC_HAS_PROCESS(Sink);
    explicit Sink(sc_core::sc_module_name name) : sc_module(name) {
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
    sc_core::sc_signal<bool>    valid;
    sc_core::sc_signal<bool>    ready;
    sc_core::sc_signal<uint64_t> data;

    CommandProcessorPv cp("cp");
    Sink              sink("sink");

    cp.clk(clk);          cp.rst_n(rst_n);
    cp.cmd_valid_o(valid); cp.cmd_ready_i(ready); cp.cmd_data_o(data);

    sink.clk(clk);        sink.rst_n(rst_n);
    sink.valid(valid);    sink.ready(ready);    sink.data(data);

    // Assert reset, then deassert.
    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // Enqueue three jobs (use bogus pointers as data words).
    void* jobs[3] = {
        reinterpret_cast<void*>(0xAAAAAAAA00000001ull),
        reinterpret_cast<void*>(0xAAAAAAAA00000002ull),
        reinterpret_cast<void*>(0xAAAAAAAA00000003ull),
    };
    for (auto* j : jobs) cp.enqueue(j);

    sc_core::sc_start(500, sc_core::SC_NS);

    if (sink.seen.size() < 3) {
        std::fprintf(stderr, "FAIL: sink saw only %zu cmds\n", sink.seen.size());
        return 1;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (sink.seen[i] != reinterpret_cast<uint64_t>(jobs[i])) {
            std::fprintf(stderr, "FAIL: cmd %zu = 0x%llx vs 0x%llx\n",
                         i, (unsigned long long)sink.seen[i],
                         (unsigned long long)reinterpret_cast<uint64_t>(jobs[i]));
            return 1;
        }
    }
    std::printf("PASS — saw %zu cmds @ %s\n",
                sink.seen.size(), sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
