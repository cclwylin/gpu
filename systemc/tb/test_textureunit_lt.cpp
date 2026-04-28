// TLM-LT TextureUnit smoke. 2×2 RGBA8 (R,G,B,W) texture, NEAREST,
// 4 sample requests at texel centres → assert per-request RGBA.

#include <cmath>
#include <cstdio>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu/texture.h"
#include "gpu_systemc/textureunit_lt.h"

using namespace gpu::systemc;

SC_MODULE(Driver) {
    tlm_utils::simple_initiator_socket<Driver> initiator;
    SC_HAS_PROCESS(Driver);
    explicit Driver(sc_core::sc_module_name n)
        : sc_module(n), initiator("initiator") {}
    bool post(TextureJob* job) {
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
    Driver        drv("drv");
    TextureUnitLt tmu("tmu");
    drv.initiator.bind(tmu.target);

    gpu::Texture tex;
    tex.width = 2; tex.height = 2;
    tex.format = gpu::Texture::RGBA8;
    tex.filter = gpu::Texture::NEAREST;
    tex.wrap_s = gpu::Texture::CLAMP;
    tex.wrap_t = gpu::Texture::CLAMP;
    tex.texels = {
        0xFF0000FFu, 0xFF00FF00u,   // row 0: red,  green
        0xFFFF0000u, 0xFFFFFFFFu,   // row 1: blue, white
    };

    TextureJob job;
    job.tex = &tex;
    job.requests = { {0.25f, 0.25f}, {0.75f, 0.25f},
                     {0.25f, 0.75f}, {0.75f, 0.75f} };

    if (!drv.post(&job)) { std::fprintf(stderr, "FAIL: TMU LT err\n"); return 1; }
    if (job.samples.size() != 4) {
        std::fprintf(stderr, "FAIL: samples=%zu\n", job.samples.size()); return 1;
    }
    auto chan = [&](size_t i, int c) { return job.samples[i][c]; };
    if (chan(0, 0) < 0.9f) { std::fprintf(stderr, "FAIL R\n"); return 1; }
    if (chan(1, 1) < 0.9f) { std::fprintf(stderr, "FAIL G\n"); return 1; }
    if (chan(2, 2) < 0.9f) { std::fprintf(stderr, "FAIL B\n"); return 1; }
    if (chan(3, 0) < 0.9f || chan(3, 1) < 0.9f || chan(3, 2) < 0.9f) {
        std::fprintf(stderr, "FAIL W\n"); return 1;
    }
    std::printf("PASS — TMU_lt 4 samples @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
