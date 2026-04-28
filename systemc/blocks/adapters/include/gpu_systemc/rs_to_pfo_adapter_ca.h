#pragma once
#include <deque>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 31 — RS → PFO payload-conversion adapter.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (RasterJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (PfoJob*)
//
// RS_ca emits a RasterJob* whose `fragments` is a flat list of
// per-pixel RasterFragment records. PFO_ca consumes a PfoJob{ctx,
// quad} per 2×2 quad. This adapter:
//   1. Groups RS fragments by (x>>1, y>>1) into Quads, lane index
//      = (y&1)*2 + (x&1). Empty lanes get coverage_mask = 0.
//   2. Emits one PfoJob per non-empty quad, sequentially. The
//      `ctx` field is set to the constructor-provided Context*,
//      which owns the framebuffer / draw state.
//   3. Adapter holds the Quad and PfoJob storage; pointers handed
//      downstream remain valid until the next RasterJob is accepted.
//
// Constructor knob: ctx (Context*) — required for PFO to know which
// framebuffer / depth / stencil / blend state to operate on.
SC_MODULE(RsToPfoAdapterCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    gpu::Context* ctx = nullptr;

    SC_HAS_PROCESS(RsToPfoAdapterCa);
    explicit RsToPfoAdapterCa(sc_core::sc_module_name name);

    size_t emitted_quads() const { return staged_quads_.size(); }

private:
    // std::deque, not std::vector — across multiple upstream RasterJobs
    // PFO may still hold a pointer to a previous-batch Quad while the
    // adapter is laying down the next batch. Clearing/reallocating a
    // vector under those reads segfaults in many-triangle scenes.
    std::deque<gpu::Quad> staged_quads_;
    std::deque<PfoJob>    pfo_jobs_;
    void thread();
};

}  // namespace gpu::systemc
