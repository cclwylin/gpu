#include "gpu_systemc/pa_to_rs_adapter_ca.h"

namespace gpu::systemc {

PaToRsAdapterCa::PaToRsAdapterCa(sc_core::sc_module_name name)
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

void PaToRsAdapterCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        PrimAssemblyJob* const pa_job =
            reinterpret_cast<PrimAssemblyJob*>(job_word);
        job_ready_o.write(false);

        staged_queue_.emplace_back();
        RasterJob& staged = staged_queue_.back();
        staged.fb_w = fb_w;
        staged.fb_h = fb_h;
        staged.msaa_4x = msaa_4x;
        staged.varying_count = varying_count;
        if (pa_job) staged.triangles = pa_job->triangles;
        wait();   // 1 cycle / repackage placeholder

        out_valid_o.write(true);
        out_data_o.write(reinterpret_cast<uint64_t>(&staged));
        wait();
        while (!out_ready_i.read()) wait();
        out_valid_o.write(false);
        out_data_o.write(0);
        wait();
    }
}

}  // namespace gpu::systemc
