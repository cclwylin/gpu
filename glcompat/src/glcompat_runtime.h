// Sprint 36 — internal runtime header for glcompat.
//
// Used by the per-entry-point .cpp files; not exposed to the
// example .c programs.

#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <GL/gl.h>

#include "gpu/state.h"
#include "gpu/types.h"

namespace glcompat {

using Mat4 = std::array<float, 16>;       // column-major, OpenGL convention
using Vec4 = std::array<float, 4>;
using Vec3 = std::array<float, 3>;

// 4x4 matrix helpers.
Mat4 mat4_identity();
Mat4 mat4_mul(const Mat4& a, const Mat4& b);
Vec4 mat4_apply(const Mat4& m, const Vec4& v);
Mat4 mat4_translate(float x, float y, float z);
Mat4 mat4_scale(float x, float y, float z);
Mat4 mat4_rotate(float angle_deg, float x, float y, float z);
Mat4 mat4_ortho(float l, float r, float b, float t, float n, float f);
Mat4 mat4_frustum(float l, float r, float b, float t, float n, float f);

// Per-vertex accumulator (one entry per glVertex call).
struct ImmVertex {
    Vec4 pos;          // attribute as written by glVertex (pre-MVP)
    Vec4 color;        // current glColor at the time of glVertex
    Vec3 normal;       // current glNormal
    Vec4 tex;          // current glTexCoord (s, t, r, q)
};

// Material params (one block; "front and back" in classic GL).
struct Material {
    Vec4 ambient   = {{0.2f, 0.2f, 0.2f, 1.0f}};
    Vec4 diffuse   = {{0.8f, 0.8f, 0.8f, 1.0f}};
    Vec4 specular  = {{0.0f, 0.0f, 0.0f, 1.0f}};
    Vec4 emission  = {{0.0f, 0.0f, 0.0f, 1.0f}};
    float shininess = 0.0f;
};

// Light params.
struct Light {
    bool  enabled = false;
    Vec4  ambient   = {{0.0f, 0.0f, 0.0f, 1.0f}};
    Vec4  diffuse   = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Vec4  specular  = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Vec4  position  = {{0.0f, 0.0f, 1.0f, 0.0f}};   // .w==0 → directional
};

// Texture object (very small).
struct Texture {
    int width = 0, height = 0;
    std::vector<uint32_t> texels;       // RGBA8, row-major
    bool linear = false;                // filter: NEAREST or BILINEAR
    bool wrap_repeat_s = false;
    bool wrap_repeat_t = false;
};

// Display-list element: a recorded immediate-mode command we can replay.
struct DListCmd {
    enum Kind { BEGIN, END, VERTEX, COLOR, NORMAL, TEXCOORD, NORMALIZE };
    Kind kind;
    GLenum mode = 0;     // for BEGIN
    Vec4 v;              // for VERTEX/COLOR/TEXCOORD
    Vec3 n;              // for NORMAL
};

// The big state blob. Single instance, file-scope inside glcompat_state.cpp.
struct State {
    // ---- matrix stacks ----
    GLenum matrix_mode = GL_MODELVIEW;
    std::vector<Mat4> modelview = {mat4_identity()};
    std::vector<Mat4> projection = {mat4_identity()};
    std::vector<Mat4> texmat = {mat4_identity()};

    // ---- viewport / clear ----
    int vp_x = 0, vp_y = 0, vp_w = 256, vp_h = 256;
    Vec4 clear_color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    float clear_depth = 1.0f;

    // ---- scissor (Sprint 43 — VK-GL-CTS color_clear scissored cases) ----
    int scissor_x = 0, scissor_y = 0, scissor_w = 0, scissor_h = 0;

    // ---- color write mask (Sprint 43 — VK-GL-CTS color_clear masked cases) ----
    bool color_mask[4] = {true, true, true, true};

    // ---- enables ----
    bool depth_test = false;
    bool blend = false;
    bool lighting = false;
    bool cull_face = false;
    bool color_material = false;
    bool tex2d = false;
    bool normalize = false;
    bool scissor_test = false;

    // ---- depth / blend / face ----
    GLenum depth_func = GL_LESS;
    bool   depth_write = true;
    GLenum blend_src = GL_ONE, blend_dst = GL_ZERO;
    GLenum cull_mode = GL_BACK;
    GLenum front_face = GL_CCW;
    GLenum shade_model = GL_SMOOTH;

    // ---- stencil ----
    bool   stencil_test = false;
    GLenum stencil_func = GL_ALWAYS;
    GLint  stencil_ref  = 0;
    GLuint stencil_read_mask  = 0xFFFFFFFFu;
    GLuint stencil_write_mask = 0xFFFFFFFFu;
    GLenum stencil_sfail  = GL_KEEP;
    GLenum stencil_dpfail = GL_KEEP;
    GLenum stencil_dppass = GL_KEEP;
    GLint  clear_stencil_value = 0;

    // ---- current vertex attrs ----
    Vec4 cur_color  = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Vec3 cur_normal = {{0.0f, 0.0f, 1.0f}};
    Vec4 cur_tex    = {{0.0f, 0.0f, 0.0f, 1.0f}};

    // ---- materials + lights ----
    Material material;                    // single, used for both faces
    std::array<Light, 8> lights;
    Vec4 light_model_ambient = {{0.2f, 0.2f, 0.2f, 1.0f}};

    // ---- textures ----
    std::vector<Texture> textures = {Texture{}};   // index 0 reserved
    GLuint bound_tex = 0;

    // ---- immediate-mode buffer ----
    bool   in_begin = false;
    GLenum prim     = 0;
    std::vector<ImmVertex> verts;

    // ---- display lists ----
    std::vector<std::vector<DListCmd>> dlists = {{}};   // index 0 unused
    bool in_dlist = false;
    GLuint dlist_writing = 0;
    GLenum dlist_mode = 0;

    // ---- gpu::Context (renders here) ----
    gpu::Context ctx;
    bool ctx_inited = false;

    // ---- raster-pos state (glDrawPixels / glBitmap) ----
    Vec4 raster_pos = {{0, 0, 0, 1}};       // window coords
    bool raster_valid = true;
    float pixel_zoom_x = 1.0f, pixel_zoom_y = 1.0f;

    // ---- ES 2.0 surface (Sprint 36) ----
    struct Buffer {
        GLenum target = 0;
        std::vector<uint8_t> data;
    };
    struct Shader {
        GLenum type = 0;
        std::string source;
        bool compiled = false;
    };
    struct Program {
        std::vector<GLuint> attached;
        // Resolved by glLinkProgram from a small built-in catalogue
        // (since we don't yet parse arbitrary glmark2 GLSL — see
        // PROGRESS.md follow-up #7). The runner registers
        // pre-baked ISA + ABI for each (vs_src,fs_src) pair it uses.
        int                  baked_id = -1;
        std::unordered_map<std::string, GLint> attribs;
        std::unordered_map<std::string, GLint> uniforms;
        bool linked = false;
        // Per-loc raw float storage. mat4 occupies all 16 floats;
        // smaller types use the prefix.
        std::array<std::array<float, 16>, 32> uniform_values{};
    };
    struct VertexAttrib {
        bool        enabled = false;
        GLint       size = 4;
        GLenum      type = GL_FLOAT;
        GLboolean   normalized = GL_FALSE;
        GLsizei     stride = 0;
        const void* pointer = nullptr;       // host ptr OR offset into bound VBO
        GLuint      vbo = 0;                 // VBO bound at glVertexAttribPointer time
    };
    std::vector<Buffer>  es2_buffers   = {Buffer{}};   // index 0 reserved
    std::vector<Shader>  es2_shaders   = {Shader{}};
    std::vector<Program> es2_programs  = {Program{}};
    GLuint               es2_array_buffer        = 0;  // currently bound GL_ARRAY_BUFFER
    GLuint               es2_element_array_buffer = 0; // currently bound GL_ELEMENT_ARRAY_BUFFER
    GLuint               es2_active_program       = 0;
    GLenum               es2_active_texture       = GL_TEXTURE0;     // selects es2_tex_units index
    std::array<VertexAttrib, 8> es2_attribs{};
    std::array<float,        4> es2_uniform_pool{};    // tiny scratch — runner pulls per-uniform values via baked accessors

    // ES 2.0 multi-unit texture binding (Sprint 38). Each unit holds the
    // glcompat-Texture object id currently bound to GL_TEXTURE_2D on
    // that unit. The legacy 1.x `bound_tex` mirrors unit 0 so existing
    // glTexImage2D / glTexParameteri code keeps working.
    static constexpr int kEs2NumTexUnits = 8;
    std::array<GLuint, kEs2NumTexUnits> es2_tex_units{};

    // Cached gpu::Texture mirrors of glcompat::Texture entries — so
    // that ctx.textures[N] can hold a stable pointer across draws and
    // we don't re-allocate texel storage every time.
    std::vector<gpu::Texture> es2_gpu_tex_cache;            // grows with state.textures
    std::vector<uint32_t>     es2_gpu_tex_cache_versions;   // matches per-id; bumped by glTexImage2D / glTexParameteri

    // ES 2.0 framebuffer objects (Sprint 38). FBO id 0 = default fb.
    struct FboAttachment {
        GLenum target = 0;        // 0 = empty, GL_TEXTURE_2D, GL_RENDERBUFFER
        GLuint name   = 0;        // texture or renderbuffer id
    };
    struct Fbo {
        FboAttachment color0;
        FboAttachment depth;
        FboAttachment stencil;
    };
    struct Renderbuffer {
        GLenum   format = 0;      // GL_DEPTH_COMPONENT16 / GL_STENCIL_INDEX8 / GL_RGBA / ...
        int      width = 0, height = 0;
    };
    std::vector<Fbo>           es2_fbos          = {Fbo{}};   // index 0 = default
    std::vector<Renderbuffer>  es2_renderbuffers = {Renderbuffer{}};
    GLuint                     es2_bound_fb      = 0;         // 0 = default fb

    // Backing framebuffer storage per FBO id. ctx.fb is the *currently
    // active* one (via std::swap on glBindFramebuffer). Index 0 holds
    // the default fb's storage when an FBO is bound.
    std::vector<gpu::Framebuffer> es2_fbo_storage = {gpu::Framebuffer{}};
};

State& state();

// Render-time helpers (defined in glcompat_render.cpp).
//
// Called from glEnd() after the immediate-mode buffer is full. Computes
// per-vertex colors via Gouraud lighting if enabled, transforms to NDC
// using the current MVP, builds a triangle list (triangulating GL_QUADS,
// GL_*_STRIP, GL_*_FAN), and submits to gpu::pipeline::draw.
void flush_immediate();

// Save the framebuffer to a PPM file. Path: $GLCOMPAT_OUT or "out.ppm".
void save_framebuffer();

// If $GLCOMPAT_SCENE is set, write the accumulated frame geometry as a
// .scene file matching the format `tests/conformance/scene_runner.cpp`
// + `sc_pattern_runner.cpp` parse. Each glEnd appends 3·N clip-space
// vertices (positions + colors) to the scene buffer.
void save_scene();

// Sprint 59 — idempotent atexit registrar. Call from any glcompat
// entrypoint that's certain to fire at least once per process; the
// helper checks `es2_scene_capture_enabled()` and only installs the
// hook when scene capture is actually on. deqp-gles2 doesn't go
// through GLUT so the GLUT path's explicit save_scene() never fires.
void install_scene_atexit_once();

// Append a CLEAR op to the scene-capture stream (no-op when capture is
// off). Called from glClear so mid-frame clears that mutate the live
// glcompat fb (texenv per-cell glClear etc.) are reproduced by the SC
// chain in the same order.
void scene_record_clear(uint32_t rgba8);

// Sprint 60 — same as above but with the active scissor rect + the
// 32-bit color-mask lane. Use this when the live glClear walks a
// sub-rect or non-trivial color mask. The replay applies
// `pix = (old & ~lane) | (rgba & lane)` per pixel inside [x0,x1)×[y0,y1).
// `full=true` is the legacy whole-fb fast-path. Without this hook the
// VK-GL-CTS color_clear `scissored_*` / `masked_*` cases diverged from
// sw_ref by ~100 RMSE in the SC chain replay.
void scene_record_clear_rect(uint32_t rgba8,
                             int x0, int y0, int x1, int y1,
                             uint32_t lane, bool full);

// Append a BITMAP op (the fb blit half of glBitmap). x,y are FB-space
// lower-left coords (raster_pos minus xb/yb origin); bits are the raw
// glBitmap bytes (MSB-first per byte, rows bottom-up per GL spec).
void scene_record_bitmap(int x, int y, int w, int h,
                         uint32_t color_rgba8,
                         const uint8_t* bits, size_t byte_count);

// Append depth/stencil clears so the SC chain mirrors the live fb.
void scene_record_clear_depth(float v);
void scene_record_clear_stencil(uint8_t v);

// ES 2.0 capture (Sprint 39). The ES2 path runs the VS in software per
// vertex into clip-space + the first varying (== colour for the simple
// pass-through shaders), then hands them here. Each call appends 3·N
// scene-vertex tuples to the open BATCH (matching the legacy 1.x
// flush_immediate format), so sc_pattern_runner can replay glmark2
// scenes through the cycle-accurate chain unchanged.
// Sprint 61 — generalised to N varyings (was single `varying_color`).
// `varyings[i][k]` holds vertex i's k-th varying (k in 0..n_vars-1, up
// to 7). The 1060 `fragment_ops.blend.*` cases pack vertex colour into
// varying[1] (varying[0] is gl_Position downstream); without N
// varyings the SC replay reconstructed garbage downstream.
void scene_record_es2_batch(const std::vector<gpu::Vec4f>& clip_pos,
                            const std::vector<std::array<gpu::Vec4f, 7>>& varyings,
                            int n_vars);

// Toggle ES 2.0 scene capture from runner test code (so non-GLUT
// runners can opt in without env vars). Default off; setting to true
// makes glDrawArrays / glDrawElements also push their post-VS
// clip-space + colour-varying tuples into the scene buffer.
// Setting `GLCOMPAT_SCENE` env still also turns it on.
void set_es2_scene_capture(bool on);
bool es2_scene_capture_enabled();

// Write the accumulated scene to `path`. Same payload as save_scene()
// but takes an explicit output path so non-GLUT runners (the
// glmark2_runner test exes) can dump without going through
// glutMainLoop's exit hook. No-op when no scene activity was recorded.
void save_scene_to(const std::string& path);

}  // namespace glcompat
