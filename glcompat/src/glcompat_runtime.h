// Sprint 36 — internal runtime header for glcompat.
//
// Used by the per-entry-point .cpp files; not exposed to the
// example .c programs.

#pragma once
#include <array>
#include <cstdint>
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

    // ---- enables ----
    bool depth_test = false;
    bool blend = false;
    bool lighting = false;
    bool cull_face = false;
    bool color_material = false;
    bool tex2d = false;
    bool normalize = false;

    // ---- depth / blend / face ----
    GLenum depth_func = GL_LESS;
    bool   depth_write = true;
    GLenum blend_src = GL_ONE, blend_dst = GL_ZERO;
    GLenum cull_mode = GL_BACK;
    GLenum front_face = GL_CCW;
    GLenum shade_model = GL_SMOOTH;

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

}  // namespace glcompat
