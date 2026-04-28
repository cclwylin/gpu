// TLM-LT TileBuffer + ResolveUnit chained smoke. 4×4 MSAA-4× fb;
// samples preloaded so box average gives a predictable RGBA8.

#include <cstdio>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_systemc/resolveunit_lt.h"
#include "gpu_systemc/tilebuffer_lt.h"

using namespace gpu::systemc;

SC_MODULE(Driver) {
    tlm_utils::simple_initiator_socket<Driver> initiator;
    SC_HAS_PROCESS(Driver);
    explicit Driver(sc_core::sc_module_name n)
        : sc_module(n), initiator("initiator") {}
    bool post(TileFlushJob* job) {
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
    TileBufferLt  tbf("tbf");
    ResolveUnitLt rsv("rsv");
    drv.initiator.bind(tbf.target);
    tbf.initiator.bind(rsv.target);

    gpu::Context ctx;
    ctx.fb.width = 4; ctx.fb.height = 4; ctx.fb.msaa_4x = true;
    ctx.fb.color.assign(16, 0u);
    ctx.fb.color_samples.assign(16 * 4, 0u);
    for (size_t pix = 0; pix < 16; ++pix) {
        ctx.fb.color_samples[pix * 4 + 0] = 0x000000FFu;
        ctx.fb.color_samples[pix * 4 + 1] = 0x000000FFu;
    }

    TileFlushJob job;
    job.ctx = &ctx;
    job.tile_x = 0; job.tile_y = 0; job.tile_w = 4; job.tile_h = 4;
    if (!drv.post(&job)) { std::fprintf(stderr, "FAIL: TBF/RSV LT err\n"); return 1; }

    for (size_t pix = 0; pix < 16; ++pix) {
        if (ctx.fb.color[pix] != 0x00000080u) {
            std::fprintf(stderr, "FAIL: pix[%zu] = 0x%08x\n", pix, ctx.fb.color[pix]);
            return 1;
        }
    }
    std::printf("PASS — TBF_lt -> RSV_lt resolved 4×4 @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
