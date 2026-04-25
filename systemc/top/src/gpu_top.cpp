#include "gpu_systemc/gpu_top.h"

namespace gpu::systemc {

GpuTop::GpuTop(sc_core::sc_module_name name)
    : sc_module(name), cp("cp"), vf("vf"), sc("sc"), pa("pa") {
    // CP -> VF (vertex job submission); VF -> SC (per-vertex VS execution).
    cp.initiator.bind(vf.target);
    vf.initiator.bind(sc.target);
    // PA is currently driven directly by the testbench (CP doesn't run an
    // initiator to PA in this sprint).
}

}  // namespace gpu::systemc
