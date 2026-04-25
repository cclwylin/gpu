#pragma once
#include <systemc>

#include "gpu_systemc/cp_block.h"
#include "gpu_systemc/sc_block.h"

namespace gpu::systemc {

// Top-level integration: just CP -> SC for Sprint 5.
SC_MODULE(GpuTop) {
    CommandProcessor cp;
    ShaderCore       sc;

    SC_HAS_PROCESS(GpuTop);
    explicit GpuTop(sc_core::sc_module_name name);
};

}  // namespace gpu::systemc
