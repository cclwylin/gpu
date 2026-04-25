#pragma once
#include <deque>
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 29 — SC → PA payload-conversion adapter.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (ShaderJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (PrimAssemblyJob*)
//
// SC_ca emits one ShaderJob* per VS thread. PA_ca expects a single
// PrimAssemblyJob* whose `vs_outputs[i]` holds the per-vertex VS
// outputs for one primitive. This adapter batches `batch_size`
// (default 3 = one triangle) consecutive ShaderJobs, copies each
// `job->outputs` into the staged PrimAssemblyJob, then forwards the
// pointer to the staged job downstream.
//
// Single-buffered: only one staged PrimAssemblyJob is alive at a
// time. After downstream accepts, the next batch begins. The staged
// job and its vs_outputs vector are owned by the adapter.
//
// Constructor knobs: batch_size, vp_w, vp_h (forwarded to the staged
// PrimAssemblyJob).
SC_MODULE(ScToPaAdapterCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    int batch_size = 3;
    int vp_w = 32;
    int vp_h = 32;
    bool cull_back = false;

    SC_HAS_PROCESS(ScToPaAdapterCa);
    explicit ScToPaAdapterCa(sc_core::sc_module_name name);

    // Read-only handle to the staged job — useful for tb assertions.
    const PrimAssemblyJob& staged() const {
        return staged_queue_.empty() ? sentinel_ : staged_queue_.back();
    }

private:
    // One PrimAssemblyJob per batch — single-buffered storage was racing
    // with PA_to_RS's read at the same delta-cycle. std::deque (not
    // vector) so push_back never invalidates the pointers we've already
    // handed downstream. Grows for the lifetime of the simulation —
    // sim-only memory cost.
    std::deque<PrimAssemblyJob> staged_queue_;
    PrimAssemblyJob             sentinel_;
    void thread();
};

}  // namespace gpu::systemc
