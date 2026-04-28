// Sprint 36 — glmark2 follow-up #6 first slice: scene-clear.
//
// Mirrors what real glmark2's scene-clear does (28-line stub: it just
// inherits the default Scene clear behaviour). We drive
// glClearColor + glClear via glcompat and assert every pixel matches.

#include <cstdio>
#include <cstdint>

#include <GL/gl.h>

#include "canvas_headless.h"

int main() {
    using gpu::glmark2_runner::HeadlessCanvas;

    HeadlessCanvas canvas(64, 48, /*r*/0.20f, /*g*/0.40f, /*b*/0.60f, /*a*/1.0f);

    auto px = canvas.read_back();
    if (px.size() != 64 * 48u) {
        std::fprintf(stderr, "FAIL: read_back size %zu\n", px.size());
        return 1;
    }

    // glcompat packs colour as (A<<24)|(B<<16)|(G<<8)|R with each byte
    // round((c · 255) + 0.5).
    const uint32_t expected = 0xFF99'66'33u;     // A=255 B=153 G=102 R=51
    int mismatches = 0;
    for (uint32_t v : px) if (v != expected) ++mismatches;
    if (mismatches != 0) {
        std::fprintf(stderr,
                     "FAIL: %d pixels differ (saw 0x%08x at [0], expected 0x%08x)\n",
                     mismatches, px[0], expected);
        return 1;
    }
    canvas.save_to_out_dir("clear");
    std::printf("PASS — clear painted %zu pixels with 0x%08x\n",
                px.size(), expected);
    return 0;
}
