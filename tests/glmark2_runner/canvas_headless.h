// Sprint 36 — headless canvas for glmark2 follow-up #9.
//
// glmark2's real Canvas drives EGL/GLX/X11/Wayland; we don't compile
// any of that. Instead, this Canvas class is a tiny helper that
// initialises a glcompat-backed framebuffer at the requested size and
// exposes pixel readback. Constructor calls glViewport + glClear so
// downstream scene code can act as if a real GL context were live.

#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <GL/gl.h>

namespace gpu::glmark2_runner {

class HeadlessCanvas {
public:
    HeadlessCanvas(int width, int height,
                   float clear_r = 0.0f, float clear_g = 0.0f,
                   float clear_b = 0.0f, float clear_a = 1.0f)
        : width_(width), height_(height) {
        glViewport(0, 0, width, height);
        glClearColor(clear_r, clear_g, clear_b, clear_a);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    int width()  const { return width_; }
    int height() const { return height_; }

    // Reads the current framebuffer back into an RGBA8 host buffer
    // (one uint32_t per pixel, packed (A<<24)|(B<<16)|(G<<8)|R).
    std::vector<uint32_t> read_back() const {
        std::vector<uint32_t> px(width_ * height_, 0u);
        glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        return px;
    }

    void swap() const { glFinish(); }

    // Sprint 39 — write the current readback as a P6 PPM. Skips
    // writing if `path` is empty. The runner tests forward
    // `getenv("GLMARK2_OUT_DIR")` joined with their scene name so the
    // CTest harness can populate `out/` for visual inspection.
    bool save_ppm(const std::string& path) const {
        if (path.empty()) return false;
        const auto px = read_back();
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << "P6\n" << width_ << " " << height_ << "\n255\n";
        std::vector<uint8_t> raw(width_ * height_ * 3);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                // PPM is top-down; our fb is GL bottom-up, so flip y.
                const uint32_t v = px[(height_ - 1 - y) * width_ + x];
                raw[3 * (y * width_ + x) + 0] =  v        & 0xFF; // R
                raw[3 * (y * width_ + x) + 1] = (v >>  8) & 0xFF; // G
                raw[3 * (y * width_ + x) + 2] = (v >> 16) & 0xFF; // B
            }
        }
        f.write(reinterpret_cast<const char*>(raw.data()),
                static_cast<std::streamsize>(raw.size()));
        return f.good();
    }

    // Convenience: if `GLMARK2_OUT_DIR` env is set, dump
    // `${dir}/glmark2_${scene}.sw.ppm`. Returns the chosen path
    // (empty if env not set so callers can chain assertions).
    std::string save_to_out_dir(const std::string& scene_name,
                                const char* suffix = ".sw.ppm") const {
        const char* env = std::getenv("GLMARK2_OUT_DIR");
        if (!env) return {};
        std::string p = std::string(env) + "/glmark2_" + scene_name + suffix;
        if (!save_ppm(p)) return {};
        std::fprintf(stderr, "[glmark2-runner] wrote %s\n", p.c_str());
        return p;
    }

private:
    int width_;
    int height_;
};

}  // namespace gpu::glmark2_runner
