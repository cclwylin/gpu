#pragma once
#include <systemc>

#include "gpu_systemc/commandprocessor_lt.h"
#include "gpu_systemc/primitiveassembly_lt.h"
#include "gpu_systemc/rasterizer_lt.h"
#include "gpu_systemc/shadercore_lt.h"
#include "gpu_systemc/vertexfetch_lt.h"

namespace gpu::systemc {

// Sprint 10/14 chain:
//   CP -> VF -> SC          (vertex path: VF runs VS via SC for each vertex)
//   PA, RS                  sibling targets, driven directly by tb/CP.
// Phase 1.x will splice CP through PA and RS once the dispatch logic in CP
// can handle multi-stage commands.
SC_MODULE(GpuTop) {
    CommandProcessorLt   cp;
    VertexFetchLt        vf;
    ShaderCoreLt         sc;
    PrimitiveAssemblyLt  pa;
    RasterizerLt         rs;

    SC_HAS_PROCESS(GpuTop);
    explicit GpuTop(sc_core::sc_module_name name);
};

}  // namespace gpu::systemc
