#include "gpu_systemc/rs_to_pfo_adapter_ca.h"

#include <utility>

namespace gpu::systemc {

RsToPfoAdapterCa::RsToPfoAdapterCa(sc_core::sc_module_name name)
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

void RsToPfoAdapterCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        // ---- accept one RasterJob* upstream ----
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        RasterJob* const rj = reinterpret_cast<RasterJob*>(job_word);
        job_ready_o.write(false);

        // ---- emit one quad per fragment in SCANLINE ORDER ----
        // Real GPUs group adjacent fragments into quads for derivative
        // support, but that batches across overlapping triangles and
        // changes which fragment wins under DEPTH_LESS. sw_ref iterates
        // fragments scanline-strict; for sw_ref↔SC RMSE parity, do the
        // same here. The "Quad" is then a single-active-lane container.
        const size_t base = staged_quads_.size();
        if (rj) {
            for (const auto& f : rj->fragments) {
                staged_quads_.emplace_back();
                gpu::Quad& q = staged_quads_.back();
                auto& frag = q.frags[0];
                frag.pos = {f.x, f.y};
                frag.coverage_mask = f.coverage_mask;
                frag.depth = f.depth;
                for (int v = 0; v < 4 && v < static_cast<int>(frag.varying.size()); ++v)
                    for (int k = 0; k < 4; ++k)
                        frag.varying[v][k] = f.varying[v][k];
                frag.varying_count = static_cast<uint8_t>(rj->varying_count);
            }
        }
        for (size_t i = base; i < staged_quads_.size(); ++i) {
            pfo_jobs_.emplace_back();
            pfo_jobs_.back().ctx  = ctx;
            pfo_jobs_.back().quad = &staged_quads_[i];
        }

        wait();   // 1 cycle / regroup placeholder

        // ---- emit one PfoJob per quad just appended ----
        for (size_t i = base; i < staged_quads_.size(); ++i) {
            PfoJob& pj = pfo_jobs_[i];
            out_valid_o.write(true);
            out_data_o.write(reinterpret_cast<uint64_t>(&pj));
            wait();
            while (!out_ready_i.read()) wait();
            out_valid_o.write(false);
            out_data_o.write(0);
            wait();
        }
    }
}

}  // namespace gpu::systemc
