#include "gpu_systemc/gpu_top.h"

namespace gpu::systemc {

GpuTop::GpuTop(sc_core::sc_module_name name)
    : sc_module(name),
      cp("cp"), vf("vf"), sc("sc"), pa("pa"), rs("rs"),
      tmu("tmu"), pfo("pfo") {
    // Vertex path: CP -> VF -> SC.
    cp.initiator.bind(vf.target);
    vf.initiator.bind(sc.target);

    // Multi-stage dispatch: CP routes per-cmd into PA / RS / TMU / PFO.
    cp.pa_initiator.bind(pa.target);
    cp.rs_initiator.bind(rs.target);
    cp.tmu_initiator.bind(tmu.target);
    cp.pfo_initiator.bind(pfo.target);
}

}  // namespace gpu::systemc
