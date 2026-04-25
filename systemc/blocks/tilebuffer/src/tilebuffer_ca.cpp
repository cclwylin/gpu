#include "gpu_systemc/tilebuffer_ca.h"

namespace gpu::systemc {

TileBufferCa::TileBufferCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      job_valid_i("job_valid_i"),
      job_ready_o("job_ready_o"),
      job_data_i ("job_data_i"),
      out_valid_o("out_valid_o"),
      out_ready_i("out_ready_i"),
      out_data_o ("out_data_o") {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void TileBufferCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        TileFlushJob* const job = reinterpret_cast<TileFlushJob*>(job_word);
        job_ready_o.write(false);

        // Tile-dump cycle placeholder. The original "1 cycle / pixel"
        // here was unrealistic for the example workload — it dominates
        // wall-clock once tiles get ≥128² because every flush stalls
        // the upstream chain for tens of thousands of cycles. Real TBF
        // SRAM is multi-banked and overlaps with PFO writes; for now
        // model a constant-time flush boundary so the chain can drain
        // many quads per second of simulation.
        if (job) {
            for (int i = 0; i < 16; ++i) wait();   // ~16-cycle stamp
        }

        out_valid_o.write(true);
        out_data_o.write(job_word);
        wait();
        while (!out_ready_i.read()) wait();
        out_valid_o.write(false);
        out_data_o.write(0);
        wait();
    }
}

}  // namespace gpu::systemc
