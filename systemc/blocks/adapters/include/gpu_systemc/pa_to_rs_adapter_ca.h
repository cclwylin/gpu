#pragma once
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 30 — PA → RS payload-conversion adapter.
//
// Wire shape (Phase-2 template):
//   Upstream consumer:    job_valid_i, job_ready_o, job_data_i (PrimAssemblyJob*)
//   Downstream producer:  out_valid_o, out_ready_i, out_data_o (RasterJob*)
//
// PA_ca emits a PrimAssemblyJob* whose `triangles` are screen-space.
// RS_ca consumes a RasterJob* whose `triangles` are the same shape
// plus framebuffer / MSAA / varying-count metadata. This adapter
// shallow-copies `triangles` and stamps the framebuffer attributes
// from constructor knobs.
//
// Single-buffered: only one staged RasterJob alive at a time.
SC_MODULE(PaToRsAdapterCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    sc_core::sc_in<bool>      job_valid_i;
    sc_core::sc_out<bool>     job_ready_o;
    sc_core::sc_in<uint64_t>  job_data_i;

    sc_core::sc_out<bool>     out_valid_o;
    sc_core::sc_in<bool>      out_ready_i;
    sc_core::sc_out<uint64_t> out_data_o;

    int  fb_w = 32;
    int  fb_h = 32;
    bool msaa_4x = false;
    int  varying_count = 0;

    SC_HAS_PROCESS(PaToRsAdapterCa);
    explicit PaToRsAdapterCa(sc_core::sc_module_name name);

    const RasterJob& staged() const { return staged_; }

private:
    RasterJob staged_;
    void thread();
};

}  // namespace gpu::systemc
