#include "gpu_systemc/perfmonitorunit_ca.h"

namespace gpu::systemc {

PerfMonitorUnitCa::PerfMonitorUnitCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      job_valid_i("job_valid_i"),
      job_ready_o("job_ready_o"),
      job_data_i ("job_data_i"),
      out_valid_o("out_valid_o"),
      out_ready_i("out_ready_i"),
      out_data_o ("out_data_o"),
      cycles_("cycles") {
    SC_CTHREAD(counter_thread, clk.pos());
    reset_signal_is(rst_n, false);
    SC_CTHREAD(access_thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void PerfMonitorUnitCa::counter_thread() {
    cycles_.write(0);
    while (true) {
        cycles_.write(cycles_.read() + 1);
        wait();
    }
}

void PerfMonitorUnitCa::access_thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        PmuJob* const req = reinterpret_cast<PmuJob*>(job_word);
        job_ready_o.write(false);

        if (req) req->cycles = cycles_.read();
        wait();

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
