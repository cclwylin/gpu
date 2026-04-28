// TLM-LT sideband blocks: CSR write/read roundtrip + PMU cycle snapshot.

#include <cstdio>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu_systemc/controlstatusregister_lt.h"
#include "gpu_systemc/perfmonitorunit_lt.h"

using namespace gpu::systemc;

template <typename Job>
SC_MODULE(GenericDriver) {
    tlm_utils::simple_initiator_socket<GenericDriver<Job>> initiator;
    SC_HAS_PROCESS(GenericDriver<Job>);
    explicit GenericDriver(sc_core::sc_module_name n)
        : sc_module(n), initiator("initiator") {}
    bool post(Job* req) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(req));
        trans.set_data_length(0);
        trans.set_streaming_width(0);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator->b_transport(trans, delay);
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }
};

int sc_main(int /*argc*/, char** /*argv*/) {
    GenericDriver<CsrJob>   csr_drv("csr_drv");
    GenericDriver<PmuJob>   pmu_drv("pmu_drv");
    ControlStatusRegisterLt csr("csr");
    PerfMonitorUnitLt       pmu("pmu");

    csr_drv.initiator.bind(csr.target);
    pmu_drv.initiator.bind(pmu.target);

    // CSR write reg 5 = 0xDEADBEEF, read back, assert match.
    CsrJob w; w.is_write = true; w.reg_idx = 5; w.value = 0xDEADBEEFu;
    if (!csr_drv.post(&w)) { std::fprintf(stderr, "FAIL: CSR write\n"); return 1; }
    CsrJob r; r.is_write = false; r.reg_idx = 5; r.value = 0;
    if (!csr_drv.post(&r)) { std::fprintf(stderr, "FAIL: CSR read\n"); return 1; }
    if (r.value != 0xDEADBEEFu) {
        std::fprintf(stderr, "FAIL: CSR readback 0x%x\n", r.value); return 1;
    }

    // PMU cycle snapshot. b_transport delay is annotated, not applied —
    // sc_start between posts is what actually advances sc_time_stamp().
    PmuJob p1;
    if (!pmu_drv.post(&p1)) { std::fprintf(stderr, "FAIL: PMU 1\n"); return 1; }
    sc_core::sc_start(sc_core::sc_time(50.0, sc_core::SC_NS));
    PmuJob p2;
    if (!pmu_drv.post(&p2)) { std::fprintf(stderr, "FAIL: PMU 2\n"); return 1; }
    if (p2.cycles <= p1.cycles) {
        std::fprintf(stderr, "FAIL: PMU cycles not monotonic (%llu -> %llu)\n",
                     (unsigned long long)p1.cycles,
                     (unsigned long long)p2.cycles);
        return 1;
    }

    std::printf("PASS — CSR roundtrip + PMU %llu->%llu @ %s\n",
                (unsigned long long)p1.cycles,
                (unsigned long long)p2.cycles,
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
