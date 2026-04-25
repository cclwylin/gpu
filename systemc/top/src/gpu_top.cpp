#include "gpu_systemc/gpu_top.h"

namespace gpu::systemc {

GpuTop::GpuTop(sc_core::sc_module_name name)
    : sc_module(name), cp("cp"), sc("sc") {
    cp.initiator.bind(sc.target);
}

}  // namespace gpu::systemc
