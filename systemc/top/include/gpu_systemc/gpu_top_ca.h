#pragma once
#include <systemc>

#include "gpu_systemc/commandprocessor_ca.h"
#include "gpu_systemc/shadercore_ca.h"
#include "gpu_systemc/vertexfetch_ca.h"

namespace gpu::systemc {

// Sprint 23 — Phase 2 cycle-accurate chain (CP_ca -> VF_ca -> SC_ca).
//
// First slice of the chip-level CA top:
//   CP_ca emits per-cmd words, VF_ca fans out N vertex jobs per cmd,
//   SC_ca runs ISA per job and forwards the same opaque pointer.
//
// PA_ca / RS_ca are *not* in the chain yet: their payload structs are
// PrimAssemblyJob / RasterJob, not ShaderJob — bridging them needs an
// adapter block (sw_ref already does this glue inside gpu::sim, so the
// chain is CP→VF→SC only until the adapter lands in Phase 2.x).
//
// The chain output is exposed at the top level so a tb sink can close
// the handshake and observe per-job pointer traffic.
SC_MODULE(GpuTopCa) {
    sc_core::sc_in<bool> clk;
    sc_core::sc_in<bool> rst_n;

    // Chain output (post-SC_ca):
    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    // Sub-blocks (public so tb can call cp.enqueue()).
    CommandProcessorCa cp;
    VertexFetchCa      vf;
    ShaderCoreCa       sc;

    SC_HAS_PROCESS(GpuTopCa);
    explicit GpuTopCa(sc_core::sc_module_name name);

private:
    // Internal interconnect.
    sc_core::sc_signal<bool>     cp_vf_valid_, cp_vf_ready_;
    sc_core::sc_signal<uint64_t> cp_vf_data_;
    sc_core::sc_signal<bool>     vf_sc_valid_, vf_sc_ready_;
    sc_core::sc_signal<uint64_t> vf_sc_data_;
};

}  // namespace gpu::systemc
