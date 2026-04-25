#include "gpu_systemc/vertexfetch_ca.h"

namespace gpu::systemc {

VertexFetchCa::VertexFetchCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      cmd_valid_i ("cmd_valid_i"),
      cmd_ready_o ("cmd_ready_o"),
      cmd_data_i  ("cmd_data_i"),
      vert_valid_o("vert_valid_o"),
      vert_ready_i("vert_ready_i"),
      vert_data_o ("vert_data_o") {
    SC_CTHREAD(thread, clk.pos());
    reset_signal_is(rst_n, false);
}

void VertexFetchCa::thread() {
    cmd_ready_o.write(false);
    vert_valid_o.write(false);
    vert_data_o.write(0);

    while (true) {
        // ---- accept one upstream command ----
        cmd_ready_o.write(true);
        do { wait(); } while (!cmd_valid_i.read());
        const uint64_t cmd = cmd_data_i.read();
        cmd_ready_o.write(false);

        // ---- emit vertices_per_cmd downstream vertex jobs ----
        // Sprint 19: data is the same opaque pointer for now (stand-in
        // for "vertex N of this draw"). Real CA model will derive
        // per-vertex addresses from the cmd payload.
        for (int v = 0; v < vertices_per_cmd; ++v) {
            vert_valid_o.write(true);
            vert_data_o.write(cmd);
            wait();                                  // hold one cycle
            while (!vert_ready_i.read()) wait();     // wait for sink
            vert_valid_o.write(false);
            vert_data_o.write(0);
            wait();                                  // 1 idle cycle between vertices
        }
    }
}

}  // namespace gpu::systemc
