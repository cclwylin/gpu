#pragma once
#include <cstdint>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// PMU (TLM-LT). Reports a cycle counter snapshot derived from the
// SystemC simulation time at request time. Real CA PMU has a clocked
// counter thread; in LT the simulation has no clock, so we divide
// sc_time_stamp() by `cycle_period` (default 1 ns to match the
// pessimistic LT delay convention used by the other blocks).
SC_MODULE(PerfMonitorUnitLt) {
    tlm_utils::simple_target_socket<PerfMonitorUnitLt> target;

    sc_core::sc_time cycle_period = sc_core::sc_time(1.0, sc_core::SC_NS);

    SC_HAS_PROCESS(PerfMonitorUnitLt);
    explicit PerfMonitorUnitLt(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
