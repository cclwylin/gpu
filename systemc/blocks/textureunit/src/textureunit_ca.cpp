#include "gpu_systemc/textureunit_ca.h"

#include "gpu/texture.h"

namespace gpu::systemc {

TextureUnitCa::TextureUnitCa(sc_core::sc_module_name name)
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

void TextureUnitCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        TextureJob* const job = reinterpret_cast<TextureJob*>(job_word);
        job_ready_o.write(false);

        // Sample each request in order.
        if (job && job->tex) {
            job->samples.resize(job->requests.size());
            for (size_t i = 0; i < job->requests.size(); ++i) {
                job->samples[i] = gpu::sample_texture(
                    *job->tex, job->requests[i].u, job->requests[i].v);
                wait();         // 1 cycle / request placeholder
            }
        }

        // Forward downstream.
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
