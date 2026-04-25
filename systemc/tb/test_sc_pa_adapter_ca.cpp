// Sprint 29 — SC_ca → SC_to_PA_adapter → PA_ca chain testbench.
//
// Three ShaderJobs run a `mov o0, c0` shader where c0 is one of the
// three vertices of a screen-space triangle (clip space). The
// adapter batches the three SC outputs into a PrimAssemblyJob, PA_ca
// emits one screen-space triangle. Asserts:
//   * pa_sink saw 1 PrimAssemblyJob*
//   * its triangles.size() == 1
//   * top-vertex screen y ≈ 27.2 on a 32×32 viewport (matches the
//     standalone PA_ca test from Sprint 21)

#include <array>
#include <cmath>
#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_compiler/asm.h"
#include "gpu_systemc/primitiveassembly_ca.h"
#include "gpu_systemc/sc_to_pa_adapter_ca.h"
#include "gpu_systemc/shadercore_ca.h"

using namespace gpu::systemc;

SC_MODULE(Source) {
    sc_core::sc_in<bool>     clk;
    sc_core::sc_in<bool>     rst_n;
    sc_core::sc_out<bool>    valid;
    sc_core::sc_in<bool>     ready;
    sc_core::sc_out<uint64_t> data;
    std::queue<uint64_t> q;
    void push(uint64_t v) { q.push(v); }
    SC_HAS_PROCESS(Source);
    explicit Source(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        valid.write(false); data.write(0);
        while (true) {
            if (q.empty()) { valid.write(false); wait(); continue; }
            valid.write(true); data.write(q.front());
            wait();
            while (!ready.read()) wait();
            q.pop();
            valid.write(false); wait();
        }
    }
};

SC_MODULE(Sink) {
    sc_core::sc_in<bool>     clk;
    sc_core::sc_in<bool>     rst_n;
    sc_core::sc_in<bool>     valid;
    sc_core::sc_out<bool>    ready;
    sc_core::sc_in<uint64_t> data;
    std::vector<uint64_t> seen;
    SC_HAS_PROCESS(Sink);
    explicit Sink(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        ready.write(false);
        while (true) { ready.write(true); wait();
            if (valid.read()) seen.push_back(data.read()); }
    }
};

int sc_main(int /*argc*/, char** /*argv*/) {
    sc_core::sc_clock           clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool>    rst_n;
    sc_core::sc_signal<bool>    a_valid, a_ready, b_valid, b_ready, c_valid, c_ready;
    sc_core::sc_signal<uint64_t> a_data, b_data, c_data;

    Source            src ("src");
    ShaderCoreCa      sc_ ("sc");
    ScToPaAdapterCa   adp ("adp");
    PrimitiveAssemblyCa pa("pa");
    Sink              sink("sink");

    // src → sc → adp → pa → sink
    src.clk(clk); src.rst_n(rst_n);
    src.valid(a_valid); src.ready(a_ready); src.data(a_data);

    sc_.clk(clk); sc_.rst_n(rst_n);
    sc_.job_valid_i(a_valid); sc_.job_ready_o(a_ready); sc_.job_data_i(a_data);
    sc_.out_valid_o(b_valid); sc_.out_ready_i(b_ready); sc_.out_data_o(b_data);

    adp.clk(clk); adp.rst_n(rst_n);
    adp.job_valid_i(b_valid); adp.job_ready_o(b_ready); adp.job_data_i(b_data);
    adp.out_valid_o(c_valid); adp.out_ready_i(c_ready); adp.out_data_o(c_data);

    sc_core::sc_signal<bool>    o_valid, o_ready;
    sc_core::sc_signal<uint64_t> o_data;
    pa.clk(clk); pa.rst_n(rst_n);
    pa.job_valid_i(c_valid); pa.job_ready_o(c_ready); pa.job_data_i(c_data);
    pa.tri_valid_o(o_valid); pa.tri_ready_i(o_ready); pa.tri_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // Shader: `mov o0, c0` — copies the per-vertex constant straight to outputs[0].
    auto a = gpu::asm_::assemble("mov o0, c0\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str()); return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    // Three vertices of the same triangle PA_ca standalone test uses.
    std::vector<ShaderJob> jobs(3);
    const std::array<gpu::sim::Vec4, 3> verts = {{
        {{ 0.0f,  0.7f, 0.0f, 1.0f}},
        {{-0.7f, -0.7f, 0.0f, 1.0f}},
        {{ 0.7f, -0.7f, 0.0f, 1.0f}},
    }};
    for (int i = 0; i < 3; ++i) {
        jobs[i].code = &code;
        jobs[i].is_vs = true;
        jobs[i].constants[0] = verts[i];
        src.push(reinterpret_cast<uint64_t>(&jobs[i]));
    }

    sc_core::sc_start(5, sc_core::SC_US);

    if (sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    const auto* paj = reinterpret_cast<const PrimAssemblyJob*>(sink.seen[0]);
    // (Sprint 39: dropped the pointer-equality check against
    // adp.staged(); with multi-buffered storage the adapter has
    // already started a fresh slot for the next batch by the time
    // we get here, so back() doesn't match what the sink received.
    // The functional checks below still pin the result tightly.)
    if (paj->triangles.size() != 1u) {
        std::fprintf(stderr, "FAIL: triangles = %zu (expected 1)\n", paj->triangles.size());
        return 1;
    }
    const float sy0 = paj->triangles[0][0][0][1];
    if (sy0 < 26.0f || sy0 > 28.0f) {
        std::fprintf(stderr, "FAIL: top y = %g (expected ~27.2)\n", sy0);
        return 1;
    }
    std::printf("PASS — SC→adapter→PA chain emitted 1 tri, top y=%g @ %s\n",
                sy0, sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
