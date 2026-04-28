#pragma once
#include <queue>
#include <systemc>

#include "gpu_systemc/payload.h"

namespace gpu::systemc {

// Sprint 18 — Phase 2 kickoff: cycle-accurate, pin-level CP.
//
// Coexists with the Phase-1 CommandProcessorLt (TLM-LT, b_transport based).
// This PV-style block uses sc_signal ports + a clocked SC_CTHREAD so the
// downstream block (in this first slice, just a buffered "FIFO sink" for
// integration testing) sees one transaction per cycle on a ready/valid
// handshake.
//
// Pattern documented here is the template for the remaining 14 blocks:
//   - sc_in/sc_out signals on the chip-internal interface
//   - SC_CTHREAD synchronous to clk + reset
//   - ready/valid handshake (AXI-stream-like)
//   - same payload structs as the LT model, just framed differently
//
// See docs/phase2_kickoff.md for the full migration plan.
SC_MODULE(CommandProcessorCa) {
    sc_core::sc_in<bool>  clk;
    sc_core::sc_in<bool>  rst_n;

    // Downstream: command-out stream (valid/ready). Payload pointer is
    // carried as an opaque void* over a 64-bit signal — TLM-LT keeps the
    // typed payload, this layer only models the wire-level handshake.
    sc_core::sc_out<bool>     cmd_valid_o;
    sc_core::sc_in<bool>      cmd_ready_i;
    sc_core::sc_out<uint64_t> cmd_data_o;     // stand-in for payload addr

    SC_HAS_PROCESS(CommandProcessorCa);
    explicit CommandProcessorCa(sc_core::sc_module_name name);

    // Driver-side enqueue, mirrors the LT version's API.
    void enqueue(void* job);

private:
    std::queue<void*> queue_;
    sc_core::sc_event event_;

    void thread();
};

}  // namespace gpu::systemc
