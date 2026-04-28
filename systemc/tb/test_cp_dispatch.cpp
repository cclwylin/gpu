// CP multi-stage dispatch (Item 5). Drives the same VertexFetchJob as
// test_tlm_hello via Stage::VF, then drives a PrimAssemblyJob via
// Stage::PA, all through the chip-level GpuTop.

#include <cstdio>
#include <systemc>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/gpu_top.h"

int sc_main(int /*argc*/, char** /*argv*/) {
    using namespace gpu::systemc;

    GpuTop top("top");

    // VS: pass-through pos + colour.
    auto a = gpu::asm_::assemble("mov o0, r0\nmov o1, r1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str()); return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    // (1) Stage::VF — original chain CP -> VF -> SC.
    VertexFetchJob vfj{};
    vfj.vs_code = &code;
    vfj.attr_count = 2;
    vfj.vertex_count = 3;
    vfj.vertices.resize(3);
    vfj.vertices[0][0] = {{ 0.0f,  0.7f, 0.0f, 1.0f}};
    vfj.vertices[1][0] = {{-0.7f, -0.7f, 0.0f, 1.0f}};
    vfj.vertices[2][0] = {{ 0.7f, -0.7f, 0.0f, 1.0f}};
    vfj.vertices[0][1] = {{1, 0, 0, 1}};
    vfj.vertices[1][1] = {{0, 1, 0, 1}};
    vfj.vertices[2][1] = {{0, 0, 1, 1}};

    top.cp.enqueue(CommandProcessorLt::Stage::VF, &vfj);
    sc_core::sc_start(1, sc_core::SC_MS);

    if (vfj.vs_outputs.size() != 3) {
        std::fprintf(stderr, "FAIL: VF didn't emit 3 vs_outputs\n"); return 1;
    }

    // (2) Stage::PA — CP routes a PrimAssemblyJob to PA via pa_initiator.
    PrimAssemblyJob paj{};
    paj.vs_outputs = vfj.vs_outputs;
    paj.vp_w = 32; paj.vp_h = 32;
    paj.cull_back = false;
    top.cp.enqueue(CommandProcessorLt::Stage::PA, &paj);
    sc_core::sc_start(1, sc_core::SC_MS);

    if (paj.triangles.size() != 1) {
        std::fprintf(stderr, "FAIL: PA emitted %zu triangles\n",
                     paj.triangles.size());
        return 1;
    }

    // (3) Stage::RS — same PA output through RS.
    RasterJob rj{};
    rj.triangles = paj.triangles;
    rj.fb_w = 32; rj.fb_h = 32;
    rj.varying_count = 1;
    top.cp.enqueue(CommandProcessorLt::Stage::RS, &rj);
    sc_core::sc_start(1, sc_core::SC_MS);

    if (rj.fragments.empty()) {
        std::fprintf(stderr, "FAIL: RS produced no fragments\n"); return 1;
    }

    std::printf("PASS — CP dispatched VF + PA + RS, %zu frags @ %s\n",
                rj.fragments.size(),
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
