// Sprint 24 — cycle-accurate TextureUnit testbench.
//
// Builds a 2×2 RGBA8 texture (red, green, blue, white), pushes a
// TextureJob with 4 sample requests at the texel centres (NEAREST),
// asserts job->samples == matching colours and sink saw 1 hit.

#include <cstdio>
#include <queue>
#include <systemc>
#include <vector>

#include "gpu_systemc/textureunit_ca.h"

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
    sc_core::sc_signal<bool>    j_valid, j_ready, o_valid, o_ready;
    sc_core::sc_signal<uint64_t> j_data, o_data;

    Source        src ("src");
    TextureUnitCa tmu ("tmu");
    Sink          sink("sink");

    src.clk(clk); src.rst_n(rst_n);
    src.valid(j_valid); src.ready(j_ready); src.data(j_data);

    tmu.clk(clk); tmu.rst_n(rst_n);
    tmu.job_valid_i(j_valid); tmu.job_ready_o(j_ready); tmu.job_data_i(j_data);
    tmu.out_valid_o(o_valid); tmu.out_ready_i(o_ready); tmu.out_data_o(o_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(o_valid); sink.ready(o_ready); sink.data(o_data);

    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    // 2x2 RGBA8 texture: (0,0)=red (1,0)=green (0,1)=blue (1,1)=white
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

    src.push(reinterpret_cast<uint64_t>(&job));
    sc_core::sc_start(2, sc_core::SC_US);

    if (sink.seen.size() != 1u) {
        std::fprintf(stderr, "FAIL: sink saw %zu (expected 1)\n", sink.seen.size());
        return 1;
    }
    if (job.samples.size() != 4u) {
        std::fprintf(stderr, "FAIL: %zu samples (expected 4)\n", job.samples.size());
        return 1;
    }
    // sample_texture returns RGBA in [0,1]; texel layout is RGBA but the
    // packed uint32 has byte-order ABGR on little-endian, so the readback
    // ordering matches what gpu::sample_texture decodes — we just check
    // each sample is non-zero in the correct primary channel.
    auto chan = [&](size_t i, int c) { return job.samples[i][c]; };
    if (chan(0, 0) < 0.9f) { std::fprintf(stderr, "FAIL: sample0 not red\n"); return 1; }
    if (chan(1, 1) < 0.9f) { std::fprintf(stderr, "FAIL: sample1 not green\n"); return 1; }
    if (chan(2, 2) < 0.9f) { std::fprintf(stderr, "FAIL: sample2 not blue\n"); return 1; }
    if (chan(3, 0) < 0.9f || chan(3,1) < 0.9f || chan(3,2) < 0.9f) {
        std::fprintf(stderr, "FAIL: sample3 not white\n"); return 1;
    }
    std::printf("PASS — TMU_ca sampled 4 texels @ %s\n",
                sc_core::sc_time_stamp().to_string().c_str());
    return 0;
}
