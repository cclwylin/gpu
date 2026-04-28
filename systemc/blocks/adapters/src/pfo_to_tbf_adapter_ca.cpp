#include "gpu_systemc/pfo_to_tbf_adapter_ca.h"

#include "gpu/state.h"

namespace gpu::systemc {

PfoToTbfAdapterCa::PfoToTbfAdapterCa(sc_core::sc_module_name name)
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

void PfoToTbfAdapterCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        // Latch ctx from incoming PfoJobs and trigger when quota hit.
        gpu::Context* ctx = nullptr;
        for (int i = 0; i < quads_per_flush; ++i) {
            job_ready_o.write(true);
            do { wait(); } while (!job_valid_i.read());
            const uint64_t job_word = job_data_i.read();
            PfoJob* const pj = reinterpret_cast<PfoJob*>(job_word);
            job_ready_o.write(false);
            if (pj && pj->ctx) ctx = pj->ctx;
            wait();
        }

        // Stage the flush over the full framebuffer.
        staged_ = TileFlushJob{};
        staged_.ctx = ctx;
        if (ctx) {
            staged_.tile_x = 0;
            staged_.tile_y = 0;
            staged_.tile_w = ctx->fb.width;
            staged_.tile_h = ctx->fb.height;
        }

        out_valid_o.write(true);
        out_data_o.write(reinterpret_cast<uint64_t>(&staged_));
        wait();
        while (!out_ready_i.read()) wait();
        out_valid_o.write(false);
        out_data_o.write(0);
        wait();
    }
}

}  // namespace gpu::systemc
