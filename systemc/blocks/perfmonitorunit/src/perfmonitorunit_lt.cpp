#include "gpu_systemc/perfmonitorunit_lt.h"

namespace gpu::systemc {

PerfMonitorUnitLt::PerfMonitorUnitLt(sc_core::sc_module_name name)
    : sc_module(name), target("target") {
    target.register_b_transport(this, &PerfMonitorUnitLt::b_transport);
}

void PerfMonitorUnitLt::b_transport(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay) {
    auto* req = reinterpret_cast<PmuJob*>(trans.get_data_ptr());
    if (!req) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const sc_core::sc_time now = sc_core::sc_time_stamp() + delay;
    const double period = cycle_period.to_seconds();
    req->cycles = (period > 0.0)
                      ? static_cast<uint64_t>(now.to_seconds() / period)
                      : 0u;
    delay += sc_core::sc_time(1.0, sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
