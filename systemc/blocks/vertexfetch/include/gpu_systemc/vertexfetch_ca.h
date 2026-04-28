#pragma once
#include <queue>
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 19 — Phase 2 cycle-accurate VertexFetch.
//
// Wire shape (matches the Phase-2 template in docs/phase2_kickoff.md):
//   Upstream consumer:    cmd_valid_i, cmd_ready_o, cmd_data_i
//   Downstream producer:  vert_valid_o, vert_ready_i, vert_data_o
//
// Sprint-19 functional minimum: each accepted upstream command produces
// `vertices_per_cmd` downstream vertex jobs in sequence. The actual
// vertex/index/attribute fetch (LT block does this via the host-side
// VertexFetchJob payload) is wired through the data signal — this CA
// model only manages the timing / handshake; payload pointer is
// pass-through.
//
// Sprint 19 does NOT yet replace the LT block in `gpu_top` — both
// flavours coexist and the integration of CP_ca → VF_ca → SC_ca chain
// will land once SC also has a CA variant (Sprint 20).
SC_MODULE(VertexFetchCa) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;

    // Upstream consumer:
    sc_core::sc_in<bool>      cmd_valid_i;
    sc_core::sc_out<bool>     cmd_ready_o;
    sc_core::sc_in<uint64_t>  cmd_data_i;

    // Downstream producer:
    sc_core::sc_out<bool>     vert_valid_o;
    sc_core::sc_in<bool>      vert_ready_i;
    sc_core::sc_out<uint64_t> vert_data_o;

    // Compile-time knob: per-cmd fan-out count (placeholder for the
    // real per-draw vertex count in Phase-2.x).
    int vertices_per_cmd = 3;

    SC_HAS_PROCESS(VertexFetchCa);
    explicit VertexFetchCa(sc_core::sc_module_name name);

private:
    void thread();
};

}  // namespace gpu::systemc
