#include "gpu_systemc/commandprocessor_pv.h"

namespace gpu::systemc {

CommandProcessorPv::CommandProcessorPv(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      cmd_valid_o("cmd_valid_o"),
      cmd_ready_i("cmd_ready_i"),
      cmd_data_o ("cmd_data_o") {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void CommandProcessorPv::enqueue(void* job) {
    queue_.push(job);
    event_.notify(sc_core::SC_ZERO_TIME);
}

void CommandProcessorPv::thread() {
    cmd_valid_o.write(false);
    cmd_data_o.write(0);
    while (true) {
        // Idle until a job lands in the queue.
        if (queue_.empty()) {
            cmd_valid_o.write(false);
            wait();           // one cycle
            continue;
        }
        void* job = queue_.front();
        cmd_valid_o.write(true);
        cmd_data_o.write(reinterpret_cast<uint64_t>(job));
        wait();               // hold valid one cycle for downstream sample
        // Wait for ready handshake.
        while (!cmd_ready_i.read()) wait();
        queue_.pop();
        cmd_valid_o.write(false);
        cmd_data_o.write(0);
    }
}

}  // namespace gpu::systemc
