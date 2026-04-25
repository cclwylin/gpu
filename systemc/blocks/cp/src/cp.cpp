#include "gpu_systemc/cp_block.h"

namespace gpu::systemc {

CommandProcessor::CommandProcessor(sc_core::sc_module_name name)
    : sc_module(name), initiator("initiator") {
    SC_THREAD(thread);
}

void CommandProcessor::enqueue(ShaderJob* job) {
    queue_.push(job);
    event_.notify(sc_core::SC_ZERO_TIME);
}

void CommandProcessor::thread() {
    while (true) {
        if (queue_.empty()) wait(event_);
        while (!queue_.empty()) {
            ShaderJob* job = queue_.front();
            queue_.pop();

            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(job));
            trans.set_data_length(sizeof(*job));
            trans.set_streaming_width(sizeof(*job));
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            initiator->b_transport(trans, delay);
            wait(delay);

            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
                SC_REPORT_ERROR("CP", "shader transaction failed");
            }
        }
    }
}

}  // namespace gpu::systemc
