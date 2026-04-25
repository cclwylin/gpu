#include "gpu_systemc/memorycontroller_ca.h"

#include <algorithm>
#include <cstring>

namespace gpu::systemc {

MemoryControllerCa::MemoryControllerCa(sc_core::sc_module_name name,
                                       size_t dram_bytes)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      job_valid_i("job_valid_i"),
      job_ready_o("job_ready_o"),
      job_data_i ("job_data_i"),
      out_valid_o("out_valid_o"),
      out_ready_i("out_ready_i"),
      out_data_o ("out_data_o"),
      dram_(dram_bytes, 0) {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void MemoryControllerCa::thread() {
    job_ready_o.write(false);
    out_valid_o.write(false);
    out_data_o.write(0);

    while (true) {
        job_ready_o.write(true);
        do { wait(); } while (!job_valid_i.read());
        const uint64_t job_word = job_data_i.read();
        MemRequest* const req = reinterpret_cast<MemRequest*>(job_word);
        job_ready_o.write(false);

        if (req && !req->fault &&
            req->addr + req->size <= dram_.size()) {
            if (req->is_write) {
                if (req->data.size() >= req->size) {
                    std::memcpy(dram_.data() + req->addr,
                                req->data.data(), req->size);
                }
            } else {
                req->data.assign(
                    dram_.begin() + static_cast<std::ptrdiff_t>(req->addr),
                    dram_.begin() + static_cast<std::ptrdiff_t>(req->addr + req->size));
            }
        }
        for (int i = 0; i < 12; ++i) wait();   // bank-latency placeholder

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
