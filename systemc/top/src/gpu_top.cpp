#include "gpu_systemc/gpu_top.h"

namespace gpu::systemc {

GpuTop::GpuTop(sc_core::sc_module_name name)
    : sc_module(name), cp("cp"), vf("vf"), sc("sc"), pa("pa"), rs("rs") {
    // CP -> VF (vertex job submission); VF -> SC (per-vertex VS execution).
    cp.initiator.bind(vf.target);
    vf.initiator.bind(sc.target);
    // PA and RS are sibling targets, driven directly by tb/CP today.
}

}  // namespace gpu::systemc
