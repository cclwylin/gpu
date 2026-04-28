#pragma once
#include <systemc>

#include "gpu_systemc/commandprocessor_lt.h"
#include "gpu_systemc/perfragmentops_lt.h"
#include "gpu_systemc/primitiveassembly_lt.h"
#include "gpu_systemc/rasterizer_lt.h"
#include "gpu_systemc/shadercore_lt.h"
#include "gpu_systemc/textureunit_lt.h"
#include "gpu_systemc/vertexfetch_lt.h"

namespace gpu::systemc {

// Phase-1 LT chip-level top.
//
//   CP -> VF -> SC          (vertex path, original chain)
//   CP -> PA, RS, TMU, PFO  (multi-stage dispatch — Item 5 follow-up)
//
// CP's per-stage initiators terminate at the matching block's target,
// so the host can call cp.enqueue(Stage::PA, &paj) and CP routes
// accordingly. All five CP initiator sockets are bound at construction;
// SystemC would otherwise reject sc_start with "sc_port not bound".
SC_MODULE(GpuTop) {
    CommandProcessorLt   cp;
    VertexFetchLt        vf;
    ShaderCoreLt         sc;
    PrimitiveAssemblyLt  pa;
    RasterizerLt         rs;
    TextureUnitLt        tmu;
    PerFragmentOpsLt     pfo;

    SC_HAS_PROCESS(GpuTop);
    explicit GpuTop(sc_core::sc_module_name name);
};

}  // namespace gpu::systemc
