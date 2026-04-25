#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 32 — PFO → TBF payload-conversion adapter (tile-flush trigger).
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (PfoJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (TileFlushJob*)
//
// PFO_ca emits one PfoJob* per processed quad. The downstream TBF_ca
// expects a TileFlushJob* per tile-flush. This adapter counts
// accepted quads and emits a TileFlushJob (covering the whole
// framebuffer for now) after every `quads_per_flush` quads.
//
// PfoJob.ctx is captured into the staged TileFlushJob so the
// downstream RSV / TBF chain can address fb.{color,depth,stencil}
// and friends.
//
// Constructor knob: quads_per_flush — number of accepted PfoJobs
// per emitted TileFlushJob. Default 1 (per-quad flush, unrealistic
// but the simplest deterministic test point). Real TBDR pipelines
// flush per binned tile, modelled in Phase 2.x.
SC_MODULE(PfoToTbfAdapterCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    int quads_per_flush = 1;

    SC_HAS_PROCESS(PfoToTbfAdapterCa);
    explicit PfoToTbfAdapterCa(sc_core::sc_module_name name);

    const TileFlushJob& staged() const { return staged_; }

private:
    TileFlushJob staged_;
    void thread();
};

}  // namespace gpu::systemc
