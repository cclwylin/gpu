// TLM-LT memory subsystem chain: MMU → L2 → MC.
// Write 16 bytes, read 16 bytes from same addr, assert read == write.

#include <cstdio>
#include <cstring>
#include <systemc>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include "gpu_systemc/l2cache_lt.h"
#include "gpu_systemc/memorycontroller_lt.h"
#include "gpu_systemc/memorymanagementunit_lt.h"

using namespace gpu::systemc;

SC_MODULE(Driver) {
    tlm_utils::simple_initiator_socket<Driver> initiator;
    SC_HAS_PROCESS(Driver);
    explicit Driver(sc_core::sc_module_name n)
        : sc_module(n), initiator("initiator") {}
    bool post(MemRequest* req) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(req));
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
    Driver                  drv("drv");
    MemoryManagementUnitLt  mmu("mmu");
    L2CacheLt               l2 ("l2");
    MemoryControllerLt      mc ("mc", 64 * 1024);

    drv.initiator.bind(mmu.target);
    mmu.initiator.bind(l2.target);
    l2.initiator.bind(mc.target);

    // Write 16 bytes at addr 0x1000.
    MemRequest wr;
    wr.addr = 0x1000; wr.size = 16; wr.is_write = true;
    wr.data.resize(16);
    for (int i = 0; i < 16; ++i) wr.data[i] = static_cast<uint8_t>(0xA0 + i);
    if (!drv.post(&wr)) { std::fprintf(stderr, "FAIL: write\n"); return 1; }
    if (wr.fault) { std::fprintf(stderr, "FAIL: write faulted\n"); return 1; }

    // Read 16 bytes at same addr.
    MemRequest rd;
    rd.addr = 0x1000; rd.size = 16; rd.is_write = false;
    if (!drv.post(&rd)) { std::fprintf(stderr, "FAIL: read\n"); return 1; }
    if (rd.data.size() != 16) {
        std::fprintf(stderr, "FAIL: rd.data=%zu\n", rd.data.size()); return 1;
    }
    for (int i = 0; i < 16; ++i) {
        if (rd.data[i] != static_cast<uint8_t>(0xA0 + i)) {
            std::fprintf(stderr, "FAIL: rd[%d]=0x%x\n", i, rd.data[i]); return 1;
        }
    }

    // Out-of-range request — MMU sets fault, MC must skip.
    MemRequest bad;
    bad.addr = (1ull << 30) + 1; bad.size = 4; bad.is_write = false;
    if (!drv.post(&bad)) { std::fprintf(stderr, "FAIL: bad post\n"); return 1; }
    if (!bad.fault) { std::fprintf(stderr, "FAIL: bad addr should fault\n"); return 1; }

    std::printf("PASS — MMU/L2/MC LT roundtrip @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
