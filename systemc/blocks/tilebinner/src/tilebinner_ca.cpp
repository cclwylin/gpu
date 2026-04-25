#include "gpu_systemc/tilebinner_ca.h"

#include <algorithm>

namespace gpu::systemc {

TileBinnerCa::TileBinnerCa(sc_core::sc_module_name name)
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

void TileBinnerCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        TileBinJob* const job = reinterpret_cast<TileBinJob*>(job_word);
        job_ready_o.write(false);

        if (job && job->tile_size > 0 && job->grid_w > 0 && job->grid_h > 0) {
            job->bin_counts.assign(static_cast<size_t>(job->grid_w) * job->grid_h, 0u);
            const int ts = job->tile_size;
            for (const auto& tri : job->triangles) {
                const int tx0 = std::max(0, tri.xmin / ts);
                const int ty0 = std::max(0, tri.ymin / ts);
                const int tx1 = std::min(job->grid_w - 1, tri.xmax / ts);
                const int ty1 = std::min(job->grid_h - 1, tri.ymax / ts);
                for (int ty = ty0; ty <= ty1; ++ty) {
                    for (int tx = tx0; tx <= tx1; ++tx) {
                        ++job->bin_counts[ty * job->grid_w + tx];
                        wait();   // 1 cycle / tile-update placeholder
                    }
                }
            }
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
