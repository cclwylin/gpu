#pragma once
#include <systemc>

#include "gpu_systemc/commandprocessor.h"
#include "gpu_systemc/primitiveassembly.h"
#include "gpu_systemc/rasterizer.h"
#include "gpu_systemc/shadercore.h"
#include "gpu_systemc/vertexfetch.h"

namespace gpu::systemc {

// Sprint 10/14 chain:
//   CP -> VF -> SC          (vertex path: VF runs VS via SC for each vertex)
//   PA, RS                  sibling targets, driven directly by tb/CP.
// Phase 1.x will splice CP through PA and RS once the dispatch logic in CP
// can handle multi-stage commands.
SC_MODULE(GpuTop) {
    CommandProcessor   cp;
    VertexFetch        vf;
    ShaderCore         sc;
    PrimitiveAssembly  pa;
    Rasterizer         rs;

    SC_HAS_PROCESS(GpuTop);
    explicit GpuTop(sc_core::sc_module_name name);
};

}  // namespace gpu::systemc
