// Sprint 10 SystemC TLM smoke: drive the CP -> VF -> SC chain with a small
// 3-vertex draw, then call PA directly to confirm screen-space transform.
//
// Skipped on macOS+GCC due to libc++/libstdc++ ABI mismatch (see
// systemc/CMakeLists.txt). Runs in the project Docker image.

#include <cmath>
#include <cstdio>
#include <systemc>
#include <tlm.h>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/gpu_top.h"

int sc_main(int /*argc*/, char** /*argv*/) {
    using namespace gpu::systemc;

    GpuTop top("top");

    // Pass-through VS: o0 = r0 (clip pos), o1 = r1 (varying colour).
    auto a = gpu::asm_::assemble("mov o0, r0\nmov o1, r1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str());
        return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

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

    top.cp.enqueue(&vfj);
    sc_core::sc_start(1, sc_core::SC_MS);

    if (vfj.vs_outputs.size() != 3) {
        std::fprintf(stderr, "FAIL: vs_outputs size %zu\n", vfj.vs_outputs.size());
        return 1;
    }
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-5f; };
    for (int v = 0; v < 3; ++v) {
        for (int k = 0; k < 4; ++k) {
            if (!near(vfj.vs_outputs[v][0][k], vfj.vertices[v][0][k])) {
                std::fprintf(stderr, "FAIL: vs_out[%d][0][%d] = %g vs %g\n",
                             v, k, vfj.vs_outputs[v][0][k], vfj.vertices[v][0][k]);
                return 1;
            }
        }
    }

    auto post = [](auto& target_socket, void* job) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(job));
        trans.set_data_length(0);
        trans.set_streaming_width(0);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        target_socket->b_transport(trans, delay);
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    };

    // Drive PA directly with VS outputs and a 32x32 viewport.
    PrimAssemblyJob paj{};
    paj.vs_outputs = vfj.vs_outputs;
    paj.vp_w = 32; paj.vp_h = 32;
    paj.cull_back = false;
    if (!post(top.pa.target, &paj)) {
        std::fprintf(stderr, "FAIL: PA transaction\n"); return 1;
    }
    if (paj.triangles.size() != 1) {
        std::fprintf(stderr, "FAIL: triangles=%zu\n", paj.triangles.size()); return 1;
    }
    float sy0 = paj.triangles[0][0][0][1];
    if (sy0 < 26.0f || sy0 > 28.0f) {
        std::fprintf(stderr, "FAIL: PA screen y=%g (expected ~27.2)\n", sy0);
        return 1;
    }

    // Drive RS with PA's screen-space triangle.
    RasterJob rj{};
    rj.triangles = paj.triangles;
    rj.fb_w = 32; rj.fb_h = 32;
    rj.varying_count = 1;     // VS emitted o0 (pos) + o1 (varying[0])
    if (!post(top.rs.target, &rj)) {
        std::fprintf(stderr, "FAIL: RS transaction\n"); return 1;
    }
    if (rj.fragments.empty()) {
        std::fprintf(stderr, "FAIL: RS produced no fragments\n"); return 1;
    }
    std::printf("RS emitted %zu fragments\n", rj.fragments.size());

    std::printf("PASS @ %s\n", sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
