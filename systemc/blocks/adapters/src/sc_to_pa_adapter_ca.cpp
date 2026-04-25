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
        // Wait for the first vertex of the next batch BEFORE touching
        // staged_. This avoids racing with the downstream consumer
        // (PA) on the previous batch — single-buffered storage.
        // Phase 2.x: switch to a ping-pong / N-deep ring to avoid the
        // adapter throughput-stalling on consumer back-pressure.
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        ShaderJob* const first =
            reinterpret_cast<ShaderJob*>(job_data_i.read());
        job_ready_o.write(false);

        // Now safe to repaint staged_ for this batch.
        staged_ = PrimAssemblyJob{};
        staged_.vp_w = vp_w;
        staged_.vp_h = vp_h;
        staged_.cull_back = cull_back;
        staged_.vs_outputs.assign(batch_size, {});
        if (first) staged_.vs_outputs[0] = first->outputs;
        wait();

        for (int i = 1; i < batch_size; ++i) {
            job_ready_o.write(true);
            do { wait(); } while (!job_valid_i.read());
            const uint64_t job_word = job_data_i.read();
            ShaderJob* const sj = reinterpret_cast<ShaderJob*>(job_word);
            job_ready_o.write(false);
            if (sj) staged_.vs_outputs[i] = sj->outputs;
            wait();   // 1 cycle / latched vertex
        }

        // Forward the pointer to the staged batch downstream.
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
