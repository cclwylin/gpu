#include "gpu_systemc/rs_to_pfo_adapter_ca.h"

#include <map>
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

        // ---- group fragments into quads ----
        std::map<std::pair<int,int>, gpu::Quad> bucket;
        if (rj) {
            for (const auto& f : rj->fragments) {
                const int qx = f.x >> 1;
                const int qy = f.y >> 1;
                const int lane = ((f.y & 1) << 1) | (f.x & 1);
                auto& q = bucket[{qy, qx}];
                auto& frag = q.frags[lane];
                frag.pos = {f.x, f.y};
                frag.coverage_mask = f.coverage_mask;
                frag.depth = f.depth;
                for (int v = 0; v < 4 && v < static_cast<int>(frag.varying.size()); ++v)
                    for (int k = 0; k < 4; ++k)
                        frag.varying[v][k] = f.varying[v][k];
                frag.varying_count = static_cast<uint8_t>(rj->varying_count);
            }
        }
        // Append this batch's quads to the deque. Old entries from
        // previous batches stay alive — PFO may still be dereferencing
        // their pointers as we lay down the new batch.
        const size_t base = staged_quads_.size();
        for (auto& kv : bucket) staged_quads_.push_back(std::move(kv.second));
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
