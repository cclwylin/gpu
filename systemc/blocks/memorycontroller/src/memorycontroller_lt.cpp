#include "gpu_systemc/memorycontroller_lt.h"

#include <cstring>

namespace gpu::systemc {

MemoryControllerLt::MemoryControllerLt(sc_core::sc_module_name name,
                                       size_t dram_bytes)
    : sc_module(name), target("target"), dram_(dram_bytes, 0) {
    target.register_b_transport(this, &MemoryControllerLt::b_transport);
}

void MemoryControllerLt::b_transport(tlm::tlm_generic_payload& trans,
                                     sc_core::sc_time& delay) {
    auto* req = reinterpret_cast<MemRequest*>(trans.get_data_ptr());
    if (!req) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (!req->fault && req->addr + req->size <= dram_.size()) {
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
    delay += sc_core::sc_time(12.0, sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

}  // namespace gpu::systemc
