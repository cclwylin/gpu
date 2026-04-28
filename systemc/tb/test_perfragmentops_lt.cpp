// TLM-LT PerFragmentOps smoke. 4×4 fb, no MSAA / depth / stencil / blend.
// One PfoJob with a Quad of 4 fragments at distinct pixels and colours.

#include <cstdio>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/perfragmentops_lt.h"

using namespace gpu::systemc;

SC_MODULE(Driver) {
    tlm_utils::simple_initiator_socket<Driver> initiator;
    SC_HAS_PROCESS(Driver);
    explicit Driver(sc_core::sc_module_name n)
        : sc_module(n), initiator("initiator") {}
    bool post(PfoJob* job) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(job));
        trans.set_data_length(0);
        trans.set_streaming_width(0);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        initiator->b_transport(trans, delay);
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }
};

int sc_main(int /*argc*/, char** /*argv*/) {
    Driver           drv("drv");
    PerFragmentOpsLt pfo("pfo");
    drv.initiator.bind(pfo.target);

    gpu::Context ctx;
    ctx.fb.width = 4; ctx.fb.height = 4; ctx.fb.msaa_4x = false;
    ctx.fb.color.assign(16, 0u);
    ctx.draw.depth_test = false;
    ctx.draw.depth_write = false;
    ctx.draw.stencil_test = false;
    ctx.draw.blend_enable = false;

    gpu::Quad quad;
    auto set_frag = [&](int i, int x, int y, float r, float g, float b) {
        auto& f = quad.frags[i];
        f.pos = {x, y};
        f.coverage_mask = 0x1;
        f.depth = 0.0f;
        f.varying[0] = {{r, g, b, 1.0f}};
        f.varying_count = 1;
    };
    set_frag(0, 0, 0, 1, 0, 0);
    set_frag(1, 1, 0, 0, 1, 0);
    set_frag(2, 0, 1, 0, 0, 1);
    set_frag(3, 1, 1, 1, 1, 1);

    PfoJob job;
    job.ctx  = &ctx;
    job.quad = &quad;
    if (!drv.post(&job)) { std::fprintf(stderr, "FAIL: PFO LT err\n"); return 1; }

    auto px = [&](int x, int y) { return ctx.fb.color[y * 4 + x]; };
    if ((px(0, 0) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (0,0) red byte 0x%x\n", px(0, 0)); return 1;
    }
    if (((px(1, 0) >> 8) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (1,0) green byte 0x%x\n", px(1, 0)); return 1;
    }
    if (((px(0, 1) >> 16) & 0xFF) != 0xFF) {
        std::fprintf(stderr, "FAIL: (0,1) blue byte 0x%x\n", px(0, 1)); return 1;
    }
    if (px(1, 1) != 0xFFFFFFFFu) {
        std::fprintf(stderr, "FAIL: (1,1) white pixel 0x%x\n", px(1, 1)); return 1;
    }
    std::printf("PASS — PFO_lt 4 fragments @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
