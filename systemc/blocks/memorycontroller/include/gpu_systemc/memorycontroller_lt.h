#pragma once
#include <cstdint>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// MC (TLM-LT). Owns a backing DRAM (std::vector<uint8_t>, sized by
// ctor). Per accepted MemRequest:
//   - read:  copies size bytes from dram[addr..addr+size] into req->data
//   - write: copies req->data into dram[addr..addr+size]
// Fault-flagged requests (set by MMU) are no-ops. Mirrors
// memorycontroller_ca; bank-latency placeholder stays at 12 ns.
SC_MODULE(MemoryControllerLt) {
    tlm_utils::simple_target_socket<MemoryControllerLt> target;

    SC_HAS_PROCESS(MemoryControllerLt);
    explicit MemoryControllerLt(sc_core::sc_module_name name,
                                size_t dram_bytes = 64 * 1024);

    const std::vector<uint8_t>& dram() const { return dram_; }
    std::vector<uint8_t>&       mut_dram()  { return dram_; }

private:
    std::vector<uint8_t> dram_;
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

}  // namespace gpu::systemc
