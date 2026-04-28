// Sprint 27 — MMU + L2 + MC chain testbench.
//
// Push two MemRequests through MMU → L2 → MC:
//   1) write 16 bytes at addr 0x1000
//   2) read  16 bytes from addr 0x1000 (different request object)
// Assert read-back matches the written pattern; sink saw 2 hits.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/l2cache_ca.h"
#include "gpu_systemc/memorycontroller_ca.h"
#include "gpu_systemc/memorymanagementunit_ca.h"

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

    Source                  src ("src");
    MemoryManagementUnitCa  mmu ("mmu");
    L2CacheCa               l2  ("l2");
    MemoryControllerCa      mc  ("mc");
    Sink                    sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(a_valid); src.ready(a_ready); src.data(a_data);

    mmu.clk(clk); mmu.rst_n(rst_n);
    mmu.job_valid_i(a_valid); mmu.job_ready_o(a_ready); mmu.job_data_i(a_data);
    mmu.out_valid_o(b_valid); mmu.out_ready_i(b_ready); mmu.out_data_o(b_data);

    l2.clk(clk); l2.rst_n(rst_n);
    l2.job_valid_i(b_valid); l2.job_ready_o(b_ready); l2.job_data_i(b_data);
    l2.out_valid_o(c_valid); l2.out_ready_i(c_ready); l2.out_data_o(c_data);

    mc.clk(clk); mc.rst_n(rst_n);
    mc.job_valid_i(c_valid); mc.job_ready_o(c_ready); mc.job_data_i(c_data);
    mc.out_valid_o(o_valid); mc.out_ready_i(o_ready); mc.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // Write request: 16 bytes 0x10..0x1F at addr 0x1000.
    MemRequest wreq;
    wreq.addr = 0x1000; wreq.size = 16; wreq.is_write = true;
    wreq.data.resize(16);
    for (int i = 0; i < 16; ++i) wreq.data[i] = static_cast<uint8_t>(0x10 + i);

    MemRequest rreq;
    rreq.addr = 0x1000; rreq.size = 16; rreq.is_write = false;

    src.push(reinterpret_cast<uint64_t>(&wreq));
    src.push(reinterpret_cast<uint64_t>(&rreq));

    sc_core::sc_start(5, sc_core::SC_US);

    if (sink.seen.size() != 2u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 2)\n", sink.seen.size());
        return 1;
    }
    if (rreq.data.size() != 16u) {
        std::fprintf(stderr, "FAIL: read returned %zu bytes\n", rreq.data.size());
        return 1;
    }
    for (int i = 0; i < 16; ++i) {
        if (rreq.data[i] != static_cast<uint8_t>(0x10 + i)) {
            std::fprintf(stderr, "FAIL: byte %d = 0x%02x (expected 0x%02x)\n",
                         i, rreq.data[i], 0x10 + i);
            return 1;
        }
    }
    std::printf("PASS — MMU→L2→MC write/read 16 B @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
