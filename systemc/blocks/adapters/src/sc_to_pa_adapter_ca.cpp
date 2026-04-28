#include "gpu_systemc/sc_to_pa_adapter_ca.h"

namespace gpu::systemc {

ScToPaAdapterCa::ScToPaAdapterCa(sc_core::sc_module_name name)
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

void ScToPaAdapterCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        // Allocate a fresh PrimAssemblyJob for this batch. Each batch
        // gets its own slot so PA_to_RS reading a previous staged_
        // never races with the next batch's reset.
        staged_queue_.emplace_back();
        PrimAssemblyJob& staged = staged_queue_.back();
        staged.vp_x = vp_x;
        staged.vp_y = vp_y;
        staged.vp_w = vp_w;
        staged.vp_h = vp_h;
        staged.cull_back = cull_back;
        staged.vs_outputs.assign(batch_size, {});

        for (int i = 0; i < batch_size; ++i) {
            job_ready_o.write(true);
            do { wait(); } while (!job_valid_i.read());
            const uint64_t job_word = job_data_i.read();
            ShaderJob* const sj = reinterpret_cast<ShaderJob*>(job_word);
            job_ready_o.write(false);
            if (sj) staged.vs_outputs[i] = sj->outputs;
            wait();   // 1 cycle / latched vertex
        }

        // Forward the pointer to the staged batch downstream.
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
