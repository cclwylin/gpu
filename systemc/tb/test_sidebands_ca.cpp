// Sprint 28 — combined sideband testbench: CSR + PMU + TileBinner.
//
// Three independent CA blocks driven from three independent
// Source/Sink pairs in a single sc_main; each block runs in
// parallel and is checked at the end of the simulation.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/controlstatusregister_ca.h"
#include "gpu_systemc/perfmonitorunit_ca.h"
#include "gpu_systemc/tilebinner_ca.h"

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

    // ---------- CSR wiring ----------
    sc_core::sc_signal<bool>    csr_jv, csr_jr, csr_ov, csr_or;
    sc_core::sc_signal<uint64_t> csr_jd, csr_od;
    Source                  csr_src ("csr_src");
    ControlStatusRegisterCa csr     ("csr");
    Sink                    csr_sink("csr_sink");
    csr_src.clk(clk); csr_src.rst_n(rst_n);
    csr_src.valid(csr_jv); csr_src.ready(csr_jr); csr_src.data(csr_jd);
    csr.clk(clk); csr.rst_n(rst_n);
    csr.job_valid_i(csr_jv); csr.job_ready_o(csr_jr); csr.job_data_i(csr_jd);
    csr.out_valid_o(csr_ov); csr.out_ready_i(csr_or); csr.out_data_o(csr_od);
    csr_sink.clk(clk); csr_sink.rst_n(rst_n);
    csr_sink.valid(csr_ov); csr_sink.ready(csr_or); csr_sink.data(csr_od);

    // ---------- PMU wiring ----------
    sc_core::sc_signal<bool>    pmu_jv, pmu_jr, pmu_ov, pmu_or;
    sc_core::sc_signal<uint64_t> pmu_jd, pmu_od;
    Source            pmu_src ("pmu_src");
    PerfMonitorUnitCa pmu     ("pmu");
    Sink              pmu_sink("pmu_sink");
    pmu_src.clk(clk); pmu_src.rst_n(rst_n);
    pmu_src.valid(pmu_jv); pmu_src.ready(pmu_jr); pmu_src.data(pmu_jd);
    pmu.clk(clk); pmu.rst_n(rst_n);
    pmu.job_valid_i(pmu_jv); pmu.job_ready_o(pmu_jr); pmu.job_data_i(pmu_jd);
    pmu.out_valid_o(pmu_ov); pmu.out_ready_i(pmu_or); pmu.out_data_o(pmu_od);
    pmu_sink.clk(clk); pmu_sink.rst_n(rst_n);
    pmu_sink.valid(pmu_ov); pmu_sink.ready(pmu_or); pmu_sink.data(pmu_od);

    // ---------- TileBinner wiring ----------
    sc_core::sc_signal<bool>    bin_jv, bin_jr, bin_ov, bin_or;
    sc_core::sc_signal<uint64_t> bin_jd, bin_od;
    Source       bin_src ("bin_src");
    TileBinnerCa bin     ("bin");
    Sink         bin_sink("bin_sink");
    bin_src.clk(clk); bin_src.rst_n(rst_n);
    bin_src.valid(bin_jv); bin_src.ready(bin_jr); bin_src.data(bin_jd);
    bin.clk(clk); bin.rst_n(rst_n);
    bin.job_valid_i(bin_jv); bin.job_ready_o(bin_jr); bin.job_data_i(bin_jd);
    bin.out_valid_o(bin_ov); bin.out_ready_i(bin_or); bin.out_data_o(bin_od);
    bin_sink.clk(clk); bin_sink.rst_n(rst_n);
    bin_sink.valid(bin_ov); bin_sink.ready(bin_or); bin_sink.data(bin_od);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // ---- CSR: write 0xDEADBEEF to reg 5, then read back. ----
    CsrJob csr_w; csr_w.is_write = true;  csr_w.reg_idx = 5; csr_w.value = 0xDEADBEEFu;
    CsrJob csr_r; csr_r.is_write = false; csr_r.reg_idx = 5; csr_r.value = 0;
    csr_src.push(reinterpret_cast<uint64_t>(&csr_w));
    csr_src.push(reinterpret_cast<uint64_t>(&csr_r));

    // ---- PMU: query cycles. ----
    PmuJob pmu_q;
    pmu_src.push(reinterpret_cast<uint64_t>(&pmu_q));

    // ---- TileBinner: 2 triangles, one in tile (0,0), one straddling (1,1)+(2,1). ----
    TileBinJob bin_job;
    bin_job.tile_size = 16;
    bin_job.grid_w = 4; bin_job.grid_h = 4;
    bin_job.triangles = {
        { 1,  10, 1,  10},        // bbox in tile (0,0)
        {20,  35, 20, 30},        // bbox in tiles (1,1) and (2,1)
    };
    bin_src.push(reinterpret_cast<uint64_t>(&bin_job));

    sc_core::sc_start(5, sc_core::SC_US);

    // -- assertions --
    if (csr_sink.seen.size() != 2u) {
        std::fprintf(stderr, "FAIL: csr_sink saw %zu (expected 2)\n", csr_sink.seen.size());
        return 1;
    }
    if (csr_r.value != 0xDEADBEEFu) {
        std::fprintf(stderr, "FAIL: CSR readback 0x%x\n", csr_r.value);
        return 1;
    }
    if (pmu_sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: pmu_sink saw %zu (expected 1)\n", pmu_sink.seen.size());
        return 1;
    }
    if (pmu_q.cycles == 0u) {
        std::fprintf(stderr, "FAIL: PMU cycles still 0\n");
        return 1;
    }
    if (bin_sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: bin_sink saw %zu (expected 1)\n", bin_sink.seen.size());
        return 1;
    }
    if (bin_job.bin_counts.size() != 16u) {
        std::fprintf(stderr, "FAIL: bin_counts size %zu\n", bin_job.bin_counts.size());
        return 1;
    }
    auto cnt = [&](int tx, int ty) { return bin_job.bin_counts[ty * 4 + tx]; };
    if (cnt(0,0) != 1u || cnt(1,1) != 1u || cnt(2,1) != 1u) {
        std::fprintf(stderr, "FAIL: bin (0,0)=%u (1,1)=%u (2,1)=%u\n",
                     cnt(0,0), cnt(1,1), cnt(2,1));
        return 1;
    }
    std::printf("PASS — CSR, PMU(cycles=%llu), BIN @ %s\n",
                static_cast<unsigned long long>(pmu_q.cycles),
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
