#include "gpu_systemc/commandprocessor_lt.h"

namespace gpu::systemc {

CommandProcessorLt::CommandProcessorLt(sc_core::sc_module_name name)
    : sc_module(name),
      initiator("initiator"),
      pa_initiator("pa_initiator"),
      rs_initiator("rs_initiator"),
      tmu_initiator("tmu_initiator"),
      pfo_initiator("pfo_initiator") {
    SC_THREAD(thread);
}

void CommandProcessorLt::enqueue(void* job) {
    enqueue(Stage::VF, job);
}

void CommandProcessorLt::enqueue(Stage stage, void* job) {
    queue_.push({stage, job});
    event_.notify(sc_core::SC_ZERO_TIME);
}

void CommandProcessorLt::thread() {
    auto pick = [&](Stage stage)
        -> tlm_utils::simple_initiator_socket<CommandProcessorLt>* {
        switch (stage) {
            case Stage::VF:  return &initiator;
            case Stage::PA:  return &pa_initiator;
            case Stage::RS:  return &rs_initiator;
            case Stage::TMU: return &tmu_initiator;
            case Stage::PFO: return &pfo_initiator;
        }
        return nullptr;
    };

    while (true) {
        if (queue_.empty()) wait(event_);
        while (!queue_.empty()) {
            Entry e = queue_.front();
            queue_.pop();

            auto* sock = pick(e.stage);
            if (!sock || sock->size() == 0) {
                // No bound target for this stage — skip silently so a
                // testbench can use an arbitrary subset of sockets.
                continue;
            }

            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(e.job));
            trans.set_data_length(0);   // payload identified by pointer, not size
            trans.set_streaming_width(0);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            (*sock)->b_transport(trans, delay);
            wait(delay);

            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
                SC_REPORT_ERROR("CP", "downstream transaction failed");
            }
        }
    }
}

}  // namespace gpu::systemc
