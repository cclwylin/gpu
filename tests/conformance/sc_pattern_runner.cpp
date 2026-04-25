// Sprint 34 — SystemC pattern runner.
//
// Reads a .scene file (same format as scene_runner.cpp), feeds the
// vertices through the cycle-accurate chain
//
//   src → SC_ca → SC_to_PA → PA_ca → PA_to_RS → RS_ca →
//         RS_to_PFO → PFO_ca → PFO_to_TBF → TBF_ca → RSV_ca → sink
//
// dumps the resulting fb.color as a binary P6 PPM, and prints
// `CYCLES=<n>` on stdout in a script-parseable form.
//
// Usage:
//   sc_pattern_runner <scene_file> <output.ppm>
//
// Docker-only on macOS+GCC due to the libstdc++/libc++ ABI mismatch
// in /usr/local/systemc-2.3.4 (same gate as the other systemc tests).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <systemc>
#include <vector>

#include "gpu/state.h"
#include "gpu/types.h"
#include "gpu_compiler/asm.h"

#include "gpu_systemc/pa_to_rs_adapter_ca.h"
#include "gpu_systemc/perfragmentops_ca.h"
#include "gpu_systemc/pfo_to_tbf_adapter_ca.h"
#include "gpu_systemc/primitiveassembly_ca.h"
#include "gpu_systemc/rasterizer_ca.h"
#include "gpu_systemc/resolveunit_ca.h"
#include "gpu_systemc/rs_to_pfo_adapter_ca.h"
#include "gpu_systemc/sc_to_pa_adapter_ca.h"
#include "gpu_systemc/shadercore_ca.h"
#include "gpu_systemc/tilebuffer_ca.h"

using namespace gpu::systemc;

namespace {

struct Scene {
    int width = 32;
    int height = 32;
    bool msaa = false;
    uint32_t clear_rgba = 0;
    std::vector<gpu::Vec4f> positions;
    std::vector<gpu::Vec4f> colours;
};

bool parse_scene(const std::string& path, Scene& s, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open " + path; return false; }
    std::string line;
    bool in_verts = false;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (in_verts) {
            if (line.rfind("end", 0) == 0) { in_verts = false; continue; }
            std::istringstream is(line);
            float x, y, z, w, r, g, b, a;
            if (!(is >> x >> y >> z >> w >> r >> g >> b >> a)) {
                err = "bad vertex line"; return false;
            }
            s.positions.push_back({{x, y, z, w}});
            s.colours.push_back({{r, g, b, a}});
            continue;
        }
        std::istringstream is(line);
        std::string key; is >> key;
        if      (key == "width")  is >> s.width;
        else if (key == "height") is >> s.height;
        else if (key == "msaa")   { int v; is >> v; s.msaa = (v != 0); }
        else if (key == "clear")  { uint32_t v; is >> std::hex >> v >> std::dec; s.clear_rgba = v; }
        else if (key == "verts")  in_verts = true;
        // unknown keys silently ignored — some belong to scene_runner only
    }
    if (s.positions.size() % 3 != 0 || s.positions.empty()) {
        err = "vertex count must be a positive multiple of 3";
        return false;
    }
    return true;
}

bool write_ppm(const std::string& path, const std::vector<uint32_t>& fb,
               int W, int H) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            uint32_t c = fb[static_cast<size_t>(y) * W + x];
            f.put(static_cast<char>(c & 0xFF));
            f.put(static_cast<char>((c >>  8) & 0xFF));
            f.put(static_cast<char>((c >> 16) & 0xFF));
        }
    }
    return true;
}

}  // namespace

SC_MODULE(Source) {
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;
    sc_core::sc_out<bool>     valid;
    sc_core::sc_in<bool>      ready;
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
    sc_core::sc_in<bool>      clk;
    sc_core::sc_in<bool>      rst_n;
    sc_core::sc_in<bool>      valid;
    sc_core::sc_out<bool>     ready;
    sc_core::sc_in<uint64_t>  data;
    int seen = 0;
    SC_HAS_PROCESS(Sink);
    explicit Sink(sc_core::sc_module_name n) : sc_module(n) {
        SC_CTHREAD(thread, clk.pos()); reset_signal_is(rst_n, false);
    }
    void thread() {
        ready.write(false);
        while (true) { ready.write(true); wait();
            if (valid.read()) ++seen; }
    }
};

int sc_main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <scene_file> <output.ppm>\n", argv[0]);
        return 2;
    }
    const std::string scene_path = argv[1];
    const std::string out_ppm    = argv[2];

    Scene scene;
    std::string err;
    if (!parse_scene(scene_path, scene, err)) {
        std::fprintf(stderr, "parse: %s\n", err.c_str()); return 1;
    }

    // Optional cap: TBF/RSV placeholders cost O(tile_w × tile_h) cycles
    // per flush, so high-res chains take minutes. SC_FB_MAX scales the
    // framebuffer (and the scene's NDC-derived screen-space coords are
    // unaffected — viewport is set per-block to the new dims).
    if (const char* e = std::getenv("SC_FB_MAX")) {
        const int cap = std::atoi(e);
        if (cap > 0 && (scene.width > cap || scene.height > cap)) {
            const float sx = (float)cap / scene.width;
            const float sy = (float)cap / scene.height;
            const float s = std::min(sx, sy);
            scene.width  = (int)(scene.width  * s);
            scene.height = (int)(scene.height * s);
            std::fprintf(stderr, "[sc] downscaled to %dx%d (SC_FB_MAX=%d)\n",
                         scene.width, scene.height, cap);
        }
    }

    // ---- VS: copy c0→o0 (position), c1→o1 (varying[0] = colour) ----
    auto a = gpu::asm_::assemble("mov o0, c0\nmov o1, c1\n");
    if (!a.error.empty()) {
        std::fprintf(stderr, "asm err: %s\n", a.error.c_str()); return 1;
    }
    std::vector<uint64_t> code(a.code.begin(), a.code.end());

    // ---- Wire up CA chain ----
    sc_core::sc_clock           clk("clk", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool>    rst_n;

    #define DECL_HOP(name) \
        sc_core::sc_signal<bool>     name##_valid, name##_ready; \
        sc_core::sc_signal<uint64_t> name##_data
    DECL_HOP(s0); DECL_HOP(s1); DECL_HOP(s2); DECL_HOP(s3); DECL_HOP(s4);
    DECL_HOP(s5); DECL_HOP(s6); DECL_HOP(s7); DECL_HOP(s8); DECL_HOP(s9);
    DECL_HOP(s10);
    #undef DECL_HOP

    Source              src    ("src");
    ShaderCoreCa        sc_    ("sc");
    ScToPaAdapterCa     sc_pa  ("sc_pa");
    PrimitiveAssemblyCa pa     ("pa");
    PaToRsAdapterCa     pa_rs  ("pa_rs");
    RasterizerCa        rs     ("rs");
    RsToPfoAdapterCa    rs_pfo ("rs_pfo");
    PerFragmentOpsCa    pfo    ("pfo");
    PfoToTbfAdapterCa   pfo_tbf("pfo_tbf");
    TileBufferCa        tbf    ("tbf");
    ResolveUnitCa       rsv    ("rsv");
    Sink                sink   ("sink");

    gpu::Context ctx;
    ctx.fb.width = scene.width; ctx.fb.height = scene.height;
    ctx.fb.msaa_4x = scene.msaa;
    ctx.fb.color.assign(scene.width * scene.height, scene.clear_rgba);
    if (scene.msaa)
        ctx.fb.color_samples.assign(scene.width * scene.height * 4,
                                    scene.clear_rgba);
    ctx.draw.depth_test = false;
    ctx.draw.depth_write = false;
    ctx.draw.stencil_test = false;
    ctx.draw.blend_enable = false;
    rs_pfo.ctx = &ctx;

    sc_pa.batch_size = 3;
    sc_pa.vp_w = scene.width; sc_pa.vp_h = scene.height;
    pa_rs.fb_w = scene.width; pa_rs.fb_h = scene.height;
    pa_rs.msaa_4x = scene.msaa;
    pa_rs.varying_count = 1;
    // Per-quad flush gives the maximum cycle resolution but quickly
    // bogs the chain at high resolution because the placeholder TBF/RSV
    // latencies scale with tile_w × tile_h. Auto-batch: aim for at most
    // ~256 flushes per frame regardless of triangle count. Override via
    // SC_QUADS_PER_FLUSH env var.
    int qpf = 1;
    if (const char* e = std::getenv("SC_QUADS_PER_FLUSH")) qpf = std::max(1, std::atoi(e));
    else if (scene.width * scene.height >= 16384) qpf = (scene.width * scene.height) / 256;
    pfo_tbf.quads_per_flush = qpf;

    src.clk(clk); src.rst_n(rst_n);
    src.valid(s0_valid); src.ready(s0_ready); src.data(s0_data);

    sc_.clk(clk); sc_.rst_n(rst_n);
    sc_.job_valid_i(s0_valid); sc_.job_ready_o(s0_ready); sc_.job_data_i(s0_data);
    sc_.out_valid_o(s1_valid); sc_.out_ready_i(s1_ready); sc_.out_data_o(s1_data);

    sc_pa.clk(clk); sc_pa.rst_n(rst_n);
    sc_pa.job_valid_i(s1_valid); sc_pa.job_ready_o(s1_ready); sc_pa.job_data_i(s1_data);
    sc_pa.out_valid_o(s2_valid); sc_pa.out_ready_i(s2_ready); sc_pa.out_data_o(s2_data);

    pa.clk(clk); pa.rst_n(rst_n);
    pa.job_valid_i(s2_valid); pa.job_ready_o(s2_ready); pa.job_data_i(s2_data);
    pa.tri_valid_o(s3_valid); pa.tri_ready_i(s3_ready); pa.tri_data_o(s3_data);

    pa_rs.clk(clk); pa_rs.rst_n(rst_n);
    pa_rs.job_valid_i(s3_valid); pa_rs.job_ready_o(s3_ready); pa_rs.job_data_i(s3_data);
    pa_rs.out_valid_o(s4_valid); pa_rs.out_ready_i(s4_ready); pa_rs.out_data_o(s4_data);

    rs.clk(clk); rs.rst_n(rst_n);
    rs.job_valid_i(s4_valid); rs.job_ready_o(s4_ready); rs.job_data_i(s4_data);
    rs.frag_valid_o(s5_valid); rs.frag_ready_i(s5_ready); rs.frag_data_o(s5_data);

    rs_pfo.clk(clk); rs_pfo.rst_n(rst_n);
    rs_pfo.job_valid_i(s5_valid); rs_pfo.job_ready_o(s5_ready); rs_pfo.job_data_i(s5_data);
    rs_pfo.out_valid_o(s6_valid); rs_pfo.out_ready_i(s6_ready); rs_pfo.out_data_o(s6_data);

    pfo.clk(clk); pfo.rst_n(rst_n);
    pfo.job_valid_i(s6_valid); pfo.job_ready_o(s6_ready); pfo.job_data_i(s6_data);
    pfo.out_valid_o(s7_valid); pfo.out_ready_i(s7_ready); pfo.out_data_o(s7_data);

    pfo_tbf.clk(clk); pfo_tbf.rst_n(rst_n);
    pfo_tbf.job_valid_i(s7_valid); pfo_tbf.job_ready_o(s7_ready); pfo_tbf.job_data_i(s7_data);
    pfo_tbf.out_valid_o(s8_valid); pfo_tbf.out_ready_i(s8_ready); pfo_tbf.out_data_o(s8_data);

    tbf.clk(clk); tbf.rst_n(rst_n);
    tbf.job_valid_i(s8_valid); tbf.job_ready_o(s8_ready); tbf.job_data_i(s8_data);
    tbf.out_valid_o(s9_valid); tbf.out_ready_i(s9_ready); tbf.out_data_o(s9_data);

    rsv.clk(clk); rsv.rst_n(rst_n);
    rsv.job_valid_i(s9_valid); rsv.job_ready_o(s9_ready); rsv.job_data_i(s9_data);
    rsv.out_valid_o(s10_valid); rsv.out_ready_i(s10_ready); rsv.out_data_o(s10_data);

    sink.clk(clk); sink.rst_n(rst_n);
    sink.valid(s10_valid); sink.ready(s10_ready); sink.data(s10_data);

    // ---- Reset, push every vertex as a ShaderJob, simulate ----
    rst_n.write(false);
    sc_core::sc_start(20, sc_core::SC_NS);
    rst_n.write(true);

    std::vector<ShaderJob> jobs(scene.positions.size());
    for (size_t i = 0; i < scene.positions.size(); ++i) {
        jobs[i].code = &code;
        jobs[i].is_vs = true;
        for (int k = 0; k < 4; ++k) {
            jobs[i].constants[0][k] = scene.positions[i][k];
            jobs[i].constants[1][k] = scene.colours[i][k];
        }
        src.push(reinterpret_cast<uint64_t>(&jobs[i]));
    }

    // Drain-detect: chain finished when sink.seen hasn't grown for
    // `idle_threshold` consecutive steps. The threshold has to exceed
    // the worst-case inter-flush latency, which for the current TBF/RSV
    // placeholders is O(tile_w × tile_h) cycles per flush — at 256×256
    // that's ~650 µs. Use 50 × 100 µs = 5 ms idle to comfortably outwait
    // a flush boundary, capped at max_steps so a stuck pipeline doesn't
    // simulate forever.
    const sc_core::sc_time step(100, sc_core::SC_US);
    const int max_steps      = 20000;    // 2 s sim cap = 200 M cycles
    const int idle_threshold = 50;       // 5 ms of pipeline-idle
    int prev_seen = -1, idle_steps = 0;
    for (int i = 0; i < max_steps; ++i) {
        sc_core::sc_start(step);
        if (sink.seen > 0 && sink.seen == prev_seen) {
            if (++idle_steps >= idle_threshold) break;
        } else {
            idle_steps = 0;
        }
        prev_seen = sink.seen;
    }

    // ---- Dump PPM + cycle metric ----
    if (!write_ppm(out_ppm, ctx.fb.color, scene.width, scene.height)) {
        std::fprintf(stderr, "FAIL: cannot write %s\n", out_ppm.c_str());
        return 1;
    }
    const double ns       = sc_core::sc_time_stamp().to_double();   // ps default
    const double clock_ps = sc_core::sc_time(10, sc_core::SC_NS).to_double();
    const long   cycles   = static_cast<long>(ns / clock_ps);
    int painted = 0;
    for (uint32_t c : ctx.fb.color)
        if (c != scene.clear_rgba) ++painted;
    std::printf("PPM=%s\n", out_ppm.c_str());
    std::printf("CYCLES=%ld\n", cycles);
    std::printf("FLUSHES=%d\n", sink.seen);
    std::printf("PAINTED=%d\n", painted);
    std::printf("TRIANGLES=%zu\n", scene.positions.size() / 3);
    return 0;
}
