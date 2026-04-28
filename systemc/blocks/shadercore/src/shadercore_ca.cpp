#include "gpu_systemc/shadercore_ca.h"

#include "gpu_compiler/sim.h"

namespace gpu::systemc {

ShaderCoreCa::ShaderCoreCa(sc_core::sc_module_name name)
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

void ShaderCoreCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        // ---- accept one upstream job ----
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t   job_word = job_data_i.read();
        ShaderJob* const job = reinterpret_cast<ShaderJob*>(job_word);
        job_ready_o.write(false);

        // ---- run ISA simulator on the job ----
        if (job && job->code) {
            gpu::sim::ThreadState t{};
            t.c = job->constants;
            if (job->is_vs) {
                for (int i = 0; i < job->attr_count && i < 8; ++i)
                    t.r[i] = job->attrs[i];
            } else {
                for (int i = 0; i < job->varying_in_count && i < 8; ++i)
                    t.varying[i] = job->varying_in[i];
            }
            (void)gpu::sim::execute(*job->code, t);
            job->outputs = t.o;
            job->lane_active = t.lane_active;

            // Timing placeholder: 1 cycle per executed instruction.
            const size_t inst_count = job->code->size();
            for (size_t k = 0; k < inst_count; ++k) wait();
        }

        // ---- emit downstream ----
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
