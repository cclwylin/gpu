#pragma once
#include <systemc>

#include "gpu_systemc/commandprocessor.h"
#include "gpu_systemc/primitiveassembly.h"
#include "gpu_systemc/shadercore.h"
#include "gpu_systemc/vertexfetch.h"

namespace gpu::systemc {

// Sprint 10 chain:
//   CP -> VF -> SC      (vertex path: VF runs VS via SC for each vertex)
//   PA is a sibling target driven directly by the testbench / CP for now.
SC_MODULE(GpuTop) {
    CommandProcessor   cp;
    VertexFetch        vf;
    ShaderCore         sc;
    PrimitiveAssembly  pa;

    SC_HAS_PROCESS(GpuTop);
    explicit GpuTop(sc_core::sc_module_name name);
};

}  // namespace gpu::systemc
