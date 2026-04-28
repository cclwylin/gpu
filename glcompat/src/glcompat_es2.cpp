// Sprint 36 — glcompat ES 2.0 surface (glmark2 follow-ups #6/#8/#9).
//
// Provides the gen/bind/buffer/program/shader/uniform/attrib/draw
// entrypoints required by glmark2-style code, on top of the
// existing 1.x immediate-mode glcompat runtime.
//
// Programs route through a small "baked program" catalogue (see
// glcompat_es2.h): testbench code pre-registers ISA + ABI for the
// (vs_src, fs_src) pair it uses. glCreateShader / glShaderSource /
// glCompileShader / glAttachShader / glLinkProgram then look up the
// matching baked entry — bypassing the still-incomplete GLSL → ISA
// frontend (follow-up #7).

#include "gpu_compiler/asm.h"
#include "gpu_compiler/encoding.h"
#include "gpu_compiler/glsl.h"
#include "gpu_compiler/sim.h"
#include "gpu/pipeline.h"
#include "gpu/state.h"
#include "gpu/types.h"

#include <algorithm>

#include "glcompat_runtime.h"
#include "glcompat_es2.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace glcompat::es2 {
namespace {

struct Catalog {
    std::vector<BakedProgram>                          entries;
    std::unordered_map<std::string, int>               by_key;       // vs_src+"\n--FS--\n"+fs_src -> id
};
Catalog& cat() { static Catalog c; return c; }

std::string make_key(const std::string& vs, const std::string& fs) {
    return vs + "\n--FS--\n" + fs;
}

}  // namespace

int register_baked_program(const std::string& vs_src,
                           const std::string& fs_src,
                           BakedProgram baked) {
    auto& c = cat();
    const auto key = make_key(vs_src, fs_src);
    auto it = c.by_key.find(key);
    if (it != c.by_key.end()) return 0;
    const int id = (int)c.entries.size();
    c.entries.push_back(std::move(baked));
    c.by_key[key] = id;
    return id + 1;        // non-zero token
}

int find_baked_program(const std::string& vs_src,
                       const std::string& fs_src) {
    auto& c = cat();
    auto it = c.by_key.find(make_key(vs_src, fs_src));
    return (it == c.by_key.end()) ? -1 : it->second;
}

const BakedProgram* baked_program(int id) {
    auto& c = cat();
    if (id < 0 || id >= (int)c.entries.size()) return nullptr;
    return &c.entries[id];
}

}  // namespace glcompat::es2

using glcompat::State;
using glcompat::state;

namespace {

State::Buffer&  buf(GLuint id)  { return state().es2_buffers[id];   }
State::Shader&  sh (GLuint id)  { return state().es2_shaders[id];   }
State::Program& pg (GLuint id)  { return state().es2_programs[id];  }

GLuint alloc_buffer()  { auto& s = state(); s.es2_buffers .push_back({}); return (GLuint)s.es2_buffers .size() - 1; }
GLuint alloc_shader()  { auto& s = state(); s.es2_shaders .push_back({}); return (GLuint)s.es2_shaders .size() - 1; }
GLuint alloc_program() { auto& s = state(); s.es2_programs.push_back({}); return (GLuint)s.es2_programs.size() - 1; }

GLuint& bound_target(GLenum target) {
    auto& s = state();
    return (target == GL_ELEMENT_ARRAY_BUFFER) ? s.es2_element_array_buffer
                                               : s.es2_array_buffer;
}

void ensure_ctx() {
    auto& s = state();
    if (s.ctx_inited) return;
    auto& fb = s.ctx.fb;
    fb.width  = s.vp_w > 0 ? s.vp_w : 256;
    fb.height = s.vp_h > 0 ? s.vp_h : 256;
    fb.color.assign((size_t)fb.width * fb.height, 0u);
    fb.depth.assign((size_t)fb.width * fb.height, 1.0f);
    s.ctx_inited = true;
}

}  // namespace

// ============================================================
// Buffer objects
// ============================================================
extern "C" void glGenBuffers(GLsizei n, GLuint* buffers) {
    for (GLsizei i = 0; i < n; ++i) buffers[i] = alloc_buffer();
}
extern "C" void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    for (GLsizei i = 0; i < n; ++i) {
        const GLuint id = buffers[i];
        if (id == 0 || id >= state().es2_buffers.size()) continue;
        state().es2_buffers[id] = {};      // clear in place
    }
}
extern "C" void glBindBuffer(GLenum target, GLuint buffer) {
    bound_target(target) = buffer;
    if (buffer && buffer < state().es2_buffers.size()) {
        state().es2_buffers[buffer].target = target;
    }
}
extern "C" void glBufferData(GLenum target, GLsizei size, const void* data, GLenum /*usage*/) {
    const GLuint id = bound_target(target);
    if (!id) return;
    auto& b = buf(id);
    b.data.assign(size, 0);
    if (data) std::memcpy(b.data.data(), data, size);
}
extern "C" void glBufferSubData(GLenum target, GLsizei offset, GLsizei size, const void* data) {
    const GLuint id = bound_target(target);
    if (!id || !data) return;
    auto& b = buf(id);
    if (offset + size > (GLsizei)b.data.size()) return;
    std::memcpy(b.data.data() + offset, data, size);
}
extern "C" void* glMapBuffer(GLenum target, GLenum /*access*/) {
    const GLuint id = bound_target(target);
    return id ? buf(id).data.data() : nullptr;
}
extern "C" GLboolean glUnmapBuffer(GLenum /*target*/) { return GL_TRUE; }

// ============================================================
// Shaders / programs
// ============================================================
extern "C" GLuint glCreateShader(GLenum type) {
    const GLuint id = alloc_shader();
    sh(id).type = type;
    return id;
}
extern "C" void glDeleteShader(GLuint id) {
    if (id && id < state().es2_shaders.size()) state().es2_shaders[id] = {};
}
extern "C" void glShaderSource(GLuint id, GLsizei count, const char** string,
                               const GLint* length) {
    if (!id || id >= state().es2_shaders.size()) return;
    std::string src;
    for (GLsizei i = 0; i < count; ++i) {
        if (length && length[i] >= 0) src.append(string[i], length[i]);
        else                          src.append(string[i]);
    }
    if (const char* env = std::getenv("GLCOMPAT_DUMP_SHADER_SOURCE")) {
        (void)env;
        std::fprintf(stderr, "===== shader %u (%s) =====\n%s\n=====\n",
                     id,
                     sh(id).type == GL_VERTEX_SHADER ? "VS" :
                     sh(id).type == GL_FRAGMENT_SHADER ? "FS" : "?",
                     src.c_str());
    }
    sh(id).source = std::move(src);
}
extern "C" void glCompileShader(GLuint id) {
    if (id && id < state().es2_shaders.size()) sh(id).compiled = true;
}
extern "C" void glGetShaderiv(GLuint /*id*/, GLenum pname, GLint* params) {
    if (pname == GL_COMPILE_STATUS) *params = GL_TRUE;
    else if (pname == GL_INFO_LOG_LENGTH) *params = 0;
}
extern "C" void glGetShaderInfoLog(GLuint, GLsizei buf_len, GLsizei* len, char* log) {
    if (buf_len > 0 && log) log[0] = 0;
    if (len) *len = 0;
}

extern "C" GLuint glCreateProgram(void)        { return alloc_program(); }
extern "C" void   glDeleteProgram(GLuint id)   { if (id && id < state().es2_programs.size()) state().es2_programs[id] = {}; }
extern "C" void   glAttachShader (GLuint p, GLuint sid) {
    if (!p || p >= state().es2_programs.size()) return;
    pg(p).attached.push_back(sid);
}
namespace {

// Try `gpu::glsl::compile` on the (vs, fs) pair and synthesise a baked
// entry from the result. Returns the new baked id or -1 on failure.
//
// Maps GLSL ABI metadata onto our c-bank / attrib / varying convention:
//   - attribute name -> next free attrib slot (0..7)
//   - uniform mat4   -> 4 consecutive c-bank slots
//   - other uniform  -> 1 c-bank slot
int try_glsl_compile(const std::string& vs_src, const std::string& fs_src) {
    using namespace gpu::glsl;
    CompileResult vs_r = compile(vs_src, ShaderStage::Vertex);
    if (!vs_r.error.empty()) {
        std::fprintf(stderr, "[glcompat ES2] VS compile failed (line %d): %s\n",
                     vs_r.error_line, vs_r.error.c_str());
        return -1;
    }
    // Sprint 55 — slot VS uniforms first, then FS. Compute:
    //   • the highest c-bank slot consumed by VS uniforms (mat4 takes 4),
    //     so FS uniforms come *after* VS in the shared c-bank.
    //   • the lowest c-bank slot consumed by VS literals (top-down
    //     allocation), so FS literals start one slot below.
    // Without this, VS and FS both grabbed slot 0 (uniforms) and slot 15
    // (literals) — the run_draw uniform/literal copies clobbered each
    // other and basic_shader.0 saw G=0 instead of c=1.
    int vs_uniform_top = 0;
    for (const auto& u : vs_r.uniforms) {
        const int width = (u.type == "mat4") ? 4 : 1;
        if (u.slot + width > vs_uniform_top) vs_uniform_top = u.slot + width;
    }
    int vs_literal_bottom = 32;  // Sprint 56 — c-bank widened to 32 slots
    std::vector<gpu::glsl::LiteralBinding> vs_lit_seed;
    vs_lit_seed.reserve(vs_r.literals.size());
    for (const auto& l : vs_r.literals) {
        vs_lit_seed.push_back({l.slot, l.value});
        if (l.slot < vs_literal_bottom) vs_literal_bottom = l.slot;
    }
    CompileResult fs_r = compile(fs_src, ShaderStage::Fragment,
                                 vs_uniform_top, vs_literal_bottom,
                                 vs_lit_seed);
    if (!fs_r.error.empty()) {
        std::fprintf(stderr, "[glcompat ES2] FS compile failed (line %d): %s\n",
                     fs_r.error_line, fs_r.error.c_str());
        return -1;
    }

    glcompat::es2::BakedProgram b;
    b.vs_code.assign(vs_r.code.begin(), vs_r.code.end());
    b.fs_code.assign(fs_r.code.begin(), fs_r.code.end());

    for (const auto& a : vs_r.attributes) {
        b.attrib_loc[a.name] = a.slot;
        if (a.slot + 1 > b.num_attribs) b.num_attribs = a.slot + 1;
    }
    int loc_id = 0;
    for (const auto& u : vs_r.uniforms) {
        b.uniform_loc[u.name]              = loc_id;
        b.uniform_to_const_slot[loc_id]    = u.slot;
        b.uniform_slot_count[loc_id]       = (u.type == "mat4") ? 4 : 1;
        ++loc_id;
    }
    for (const auto& u : fs_r.uniforms) {
        if (b.uniform_loc.count(u.name)) continue;
        b.uniform_loc[u.name]              = loc_id;
        b.uniform_to_const_slot[loc_id]    = u.slot;
        b.uniform_slot_count[loc_id]       = (u.type == "mat4") ? 4 : 1;
        ++loc_id;
    }
    // Samplers — exposed via uniform_loc so glUniform1i(loc, unit) keeps
    // working. Track ISA tex slot separately so the draw path can route
    // the bound glcompat texture into ctx.textures[slot].
    auto add_sampler = [&](const gpu::glsl::CompileResult::Binding& s) {
        if (b.uniform_loc.count(s.name)) return;
        b.uniform_loc[s.name]           = loc_id;
        b.sampler_to_tex_slot[loc_id]   = s.slot;
        ++loc_id;
    };
    for (const auto& s : vs_r.samplers) add_sampler(s);
    for (const auto& s : fs_r.samplers) add_sampler(s);
    int max_var = 0;
    for (const auto& v : vs_r.varyings_out) if (v.slot + 1 > max_var) max_var = v.slot + 1;
    for (const auto& v : fs_r.varyings_in)  if (v.slot + 1 > max_var) max_var = v.slot + 1;
    b.num_varyings = max_var;
    for (const auto& l : vs_r.literals) b.literals.push_back({l.slot, l.value});
    for (const auto& l : fs_r.literals) b.literals.push_back({l.slot, l.value});

    return glcompat::es2::register_baked_program(vs_src, fs_src, std::move(b)) - 1;
}

}  // namespace

extern "C" void glLinkProgram(GLuint pid) {
    if (!pid || pid >= state().es2_programs.size()) return;
    auto& p = pg(pid);
    // Pull the (vs,fs) source pair from attached shaders.
    std::string vs, fs;
    for (GLuint sid : p.attached) {
        if (!sid || sid >= state().es2_shaders.size()) continue;
        const auto& s = sh(sid);
        if (s.type == GL_VERTEX_SHADER)        vs = s.source;
        else if (s.type == GL_FRAGMENT_SHADER) fs = s.source;
    }
    p.baked_id = glcompat::es2::find_baked_program(vs, fs);
    if (p.baked_id < 0) {
        // Fall back to the GLSL ES 2.0 frontend (Sprint 37).
        p.baked_id = try_glsl_compile(vs, fs);
    }
    p.linked = (p.baked_id >= 0);
    if (p.linked) {
        const auto* baked = glcompat::es2::baked_program(p.baked_id);
        for (const auto& [name, loc] : baked->attrib_loc)  p.attribs [name] = loc;
        for (const auto& [name, loc] : baked->uniform_loc) p.uniforms[name] = loc;
    } else {
        std::fprintf(stderr,
            "[glcompat ES2] glLinkProgram: no baked program AND glsl compile "
            "failed for program %u\n", pid);
    }
}
extern "C" void glUseProgram(GLuint pid) { state().es2_active_program = pid; }
extern "C" void glGetProgramiv(GLuint pid, GLenum pname, GLint* params) {
    if (pname == GL_LINK_STATUS) {
        *params = (pid && pid < state().es2_programs.size() && pg(pid).linked) ? GL_TRUE : GL_FALSE;
    } else if (pname == GL_INFO_LOG_LENGTH) {
        *params = 0;
    }
}
extern "C" void glGetProgramInfoLog(GLuint, GLsizei buf_len, GLsizei* len, char* log) {
    if (buf_len > 0 && log) log[0] = 0;
    if (len) *len = 0;
}
extern "C" GLint glGetUniformLocation(GLuint pid, const char* name) {
    if (!pid || pid >= state().es2_programs.size()) return -1;
    auto it = pg(pid).uniforms.find(name);
    return (it == pg(pid).uniforms.end()) ? -1 : it->second;
}
extern "C" GLint glGetAttribLocation(GLuint pid, const char* name) {
    if (!pid || pid >= state().es2_programs.size()) return -1;
    auto it = pg(pid).attribs.find(name);
    return (it == pg(pid).attribs.end()) ? -1 : it->second;
}
extern "C" void glBindAttribLocation(GLuint /*pid*/, GLuint /*idx*/, const char* /*name*/) {
    // The baked program already owns the attrib mapping; bind requests
    // are honoured implicitly via the catalogue. Stub here.
}

// ============================================================
// Uniforms — store raw floats in Program.uniform_values[loc][...]
// ============================================================
namespace {
float* uniform_slot(GLint loc) {
    auto& s = state();
    const GLuint pid = s.es2_active_program;
    if (!pid || pid >= s.es2_programs.size())            return nullptr;
    if (loc < 0 || loc >= (GLint)pg(pid).uniform_values.size()) return nullptr;
    return pg(pid).uniform_values[loc].data();
}
}  // namespace

extern "C" void glUniform1i(GLint loc, GLint v) {
    if (auto* p = uniform_slot(loc)) p[0] = (float)v;
}
extern "C" void glUniform1f(GLint loc, GLfloat v) {
    if (auto* p = uniform_slot(loc)) p[0] = v;
}
// Sprint 50 — scalar float variants. Many dEQP tests call `glUniform4f`
// (and 2f/3f) directly; without these, dlsym(RTLD_DEFAULT) returns NULL,
// the call is a silent no-op, and uniforms stay at their initial 0.
extern "C" void glUniform2f(GLint loc, GLfloat v0, GLfloat v1) {
    if (auto* p = uniform_slot(loc)) { p[0] = v0; p[1] = v1; }
}
extern "C" void glUniform3f(GLint loc, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (auto* p = uniform_slot(loc)) { p[0] = v0; p[1] = v1; p[2] = v2; }
}
extern "C" void glUniform4f(GLint loc, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (auto* p = uniform_slot(loc)) { p[0] = v0; p[1] = v1; p[2] = v2; p[3] = v3; }
}
// Sprint 48 — dEQP's random-shader generator emits ivec uniforms and uses
// the i / iv setters; without these the stub returned via dlsym is NULL and
// every uniform stays at 0, which collapses the basic_shader image-cmp.
extern "C" void glUniform2i(GLint loc, GLint v0, GLint v1) {
    if (auto* p = uniform_slot(loc)) { p[0] = (float)v0; p[1] = (float)v1; }
}
extern "C" void glUniform3i(GLint loc, GLint v0, GLint v1, GLint v2) {
    if (auto* p = uniform_slot(loc)) {
        p[0] = (float)v0; p[1] = (float)v1; p[2] = (float)v2;
    }
}
extern "C" void glUniform4i(GLint loc, GLint v0, GLint v1, GLint v2, GLint v3) {
    if (auto* p = uniform_slot(loc)) {
        p[0] = (float)v0; p[1] = (float)v1; p[2] = (float)v2; p[3] = (float)v3;
    }
}
extern "C" void glUniform1iv(GLint loc, GLsizei /*count*/, const GLint* v) {
    if (auto* p = uniform_slot(loc)) p[0] = (float)v[0];
}
extern "C" void glUniform2iv(GLint loc, GLsizei /*count*/, const GLint* v) {
    if (auto* p = uniform_slot(loc)) {
        for (int i = 0; i < 2; ++i) p[i] = (float)v[i];
    }
}
extern "C" void glUniform3iv(GLint loc, GLsizei /*count*/, const GLint* v) {
    if (auto* p = uniform_slot(loc)) {
        for (int i = 0; i < 3; ++i) p[i] = (float)v[i];
    }
}
extern "C" void glUniform4iv(GLint loc, GLsizei /*count*/, const GLint* v) {
    if (auto* p = uniform_slot(loc)) {
        for (int i = 0; i < 4; ++i) p[i] = (float)v[i];
    }
}
extern "C" void glUniform1fv(GLint loc, GLsizei /*count*/, const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) p[0] = v[0];
}
extern "C" void glUniform2fv(GLint loc, GLsizei /*count*/, const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 2 * sizeof(float));
}
extern "C" void glUniform3fv(GLint loc, GLsizei /*count*/, const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 3 * sizeof(float));
}
extern "C" void glUniform4fv(GLint loc, GLsizei /*count*/, const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 4 * sizeof(float));
}
extern "C" void glUniformMatrix2fv(GLint loc, GLsizei /*count*/, GLboolean /*transpose*/,
                                   const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 4 * sizeof(float));
}
extern "C" void glUniformMatrix3fv(GLint loc, GLsizei /*count*/, GLboolean /*transpose*/,
                                   const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 9 * sizeof(float));
}
extern "C" void glUniformMatrix4fv(GLint loc, GLsizei /*count*/, GLboolean /*transpose*/,
                                   const GLfloat* v) {
    if (auto* p = uniform_slot(loc)) std::memcpy(p, v, 16 * sizeof(float));
}

// ============================================================
// Vertex attribs
// ============================================================
extern "C" void glEnableVertexAttribArray(GLuint idx) {
    if (idx < state().es2_attribs.size()) state().es2_attribs[idx].enabled = true;
}
extern "C" void glDisableVertexAttribArray(GLuint idx) {
    if (idx < state().es2_attribs.size()) state().es2_attribs[idx].enabled = false;
}
extern "C" void glVertexAttribPointer(GLuint idx, GLint size, GLenum type,
                                      GLboolean normalized, GLsizei stride,
                                      const void* pointer) {
    if (idx >= state().es2_attribs.size()) return;
    auto& a   = state().es2_attribs[idx];
    a.size    = size;
    a.type    = type;
    a.normalized = normalized;
    a.stride  = stride;
    a.pointer = pointer;
    a.vbo     = state().es2_array_buffer;
}

// ============================================================
// Draw — gather attribs, set up gpu::Context, run pipeline
// ============================================================
namespace {

const uint8_t* attrib_base(const State::VertexAttrib& a) {
    if (a.vbo && a.vbo < state().es2_buffers.size()) {
        return state().es2_buffers[a.vbo].data.data() + (size_t)(uintptr_t)a.pointer;
    }
    return reinterpret_cast<const uint8_t*>(a.pointer);
}

void run_draw(GLenum mode, GLsizei count, const uint32_t* indices) {
    auto& s = state();
    const GLuint pid = s.es2_active_program;
    if (!pid || pid >= s.es2_programs.size()) return;
    const auto& p = pg(pid);
    if (!p.linked) return;
    const auto* baked = glcompat::es2::baked_program(p.baked_id);
    if (!baked) return;

    ensure_ctx();

    // Gather per-attrib float[4] streams (gpu pipeline uses vec4 attribs).
    std::array<std::vector<gpu::Vec4f>, 8> attrib_streams;
    for (int ai = 0; ai < baked->num_attribs && ai < 8; ++ai) {
        const auto& va = s.es2_attribs[ai];
        attrib_streams[ai].resize(count);
        // With a VBO bound, pointer is an offset into VBO storage and
        // (const void*)0 (a.k.a. "offset 0") is a perfectly valid call —
        // so check va.enabled + (vbo OR client pointer), not pointer alone.
        const bool has_source = va.enabled && (va.vbo != 0 || va.pointer != nullptr);
        if (!has_source) {
            std::fill(attrib_streams[ai].begin(), attrib_streams[ai].end(),
                      gpu::Vec4f{0.f, 0.f, 0.f, 1.f});
            continue;
        }
        const uint8_t* base = attrib_base(va);
        // Sprint 51 — element size depends on type, not float. dEQP buffer
        // tests submit `GL_UNSIGNED_BYTE` + `normalized=GL_TRUE` for a
        // 3-byte color attribute; previously we hard-coded float stride
        // and read the bytes as floats, garbling every color.
        size_t elem_size = sizeof(float);
        switch (va.type) {
            case GL_BYTE: case GL_UNSIGNED_BYTE: elem_size = 1; break;
            case GL_SHORT: case GL_UNSIGNED_SHORT: elem_size = 2; break;
            case GL_FIXED: case GL_FLOAT:          elem_size = 4; break;
        }
        const GLsizei stride = va.stride ? va.stride
                                         : (GLsizei)(va.size * elem_size);
        for (GLsizei v = 0; v < count; ++v) {
            const GLsizei vidx  = indices ? (GLsizei)indices[v] : v;
            const uint8_t* raw  = base + (size_t)vidx * stride;
            gpu::Vec4f    out   = {0.f, 0.f, 0.f, 1.f};
            for (int c = 0; c < va.size && c < 4; ++c) {
                const uint8_t* p = raw + c * elem_size;
                float f = 0.f;
                switch (va.type) {
                    case GL_BYTE: {
                        const int8_t bv = static_cast<int8_t>(*p);
                        f = va.normalized ? std::max(bv / 127.0f, -1.0f) : (float)bv;
                        break;
                    }
                    case GL_UNSIGNED_BYTE: {
                        const uint8_t ub = *p;
                        f = va.normalized ? ub / 255.0f : (float)ub;
                        break;
                    }
                    case GL_SHORT: {
                        const int16_t sv = *reinterpret_cast<const int16_t*>(p);
                        f = va.normalized ? std::max(sv / 32767.0f, -1.0f) : (float)sv;
                        break;
                    }
                    case GL_UNSIGNED_SHORT: {
                        const uint16_t us = *reinterpret_cast<const uint16_t*>(p);
                        f = va.normalized ? us / 65535.0f : (float)us;
                        break;
                    }
                    case GL_FIXED:
                        // 16.16 fixed: bits / 65536.
                        f = (float)(*reinterpret_cast<const int32_t*>(p)) / 65536.0f;
                        break;
                    case GL_FLOAT: default:
                        f = *reinterpret_cast<const float*>(p);
                        break;
                }
                out[c] = f;
            }
            attrib_streams[ai][v] = out;
        }
    }

    // Pack uniforms into ctx.draw.uniforms[16] (the c-bank source for
    // both vertex_shader.cpp and fragment_shader.cpp). Each loc may
    // consume 1-4 vec4 slots; mat4 uniforms occupy 4 consecutive
    // slots in column-major order (matching GL convention).
    for (const auto& [name, loc] : baked->uniform_loc) {
        (void)name;
        auto slot_it = baked->uniform_to_const_slot.find(loc);
        if (slot_it == baked->uniform_to_const_slot.end()) continue;
        const int dst   = slot_it->second;
        auto cnt_it     = baked->uniform_slot_count.find(loc);
        const int slots = (cnt_it == baked->uniform_slot_count.end()) ? 1 : cnt_it->second;
        const float* src = p.uniform_values[loc].data();
        for (int sl = 0; sl < slots && (dst + sl) < (int)s.ctx.draw.uniforms.size(); ++sl) {
            for (int c = 0; c < 4; ++c) {
                s.ctx.draw.uniforms[dst + sl][c] = src[sl * 4 + c];
            }
        }
    }
    // Compiler-emitted float literals (see glsl::CompileResult::literals).
    for (const auto& [slot, value] : baked->literals) {
        if (slot >= 0 && slot < (int)s.ctx.draw.uniforms.size()) {
            s.ctx.draw.uniforms[slot] = {value, 0.0f, 0.0f, 0.0f};
        }
    }

    // Bind shaders + attribs into the gpu::Context.
    s.ctx.shaders.vs_binary        = &baked->vs_code;
    s.ctx.shaders.fs_binary        = &baked->fs_code;
    s.ctx.shaders.vs_attr_count    = baked->num_attribs;
    s.ctx.shaders.vs_varying_count = baked->num_varyings;
    s.ctx.shaders.fs_varying_count = baked->num_varyings;
    s.ctx.draw.vp_x = s.vp_x;
    s.ctx.draw.vp_y = s.vp_y;
    s.ctx.draw.vp_w = s.vp_w;
    s.ctx.draw.vp_h = s.vp_h;
    s.ctx.draw.primitive = (mode == GL_TRIANGLE_STRIP) ? gpu::DrawState::TRIANGLE_STRIP
                                                       : gpu::DrawState::TRIANGLES;
    for (int ai = 0; ai < baked->num_attribs && ai < 8; ++ai) {
        s.ctx.attribs[ai] = {true, 4, gpu::VertexAttribBinding::F32,
                             sizeof(gpu::Vec4f), 0, attrib_streams[ai].data()};
    }
    for (int ai = baked->num_attribs; ai < 8; ++ai) s.ctx.attribs[ai] = {};

    // Sync gpu::Texture cache from glcompat::Texture, then route each
    // sampler uniform to ctx.textures[ISA tex slot].
    s.es2_gpu_tex_cache.assign(s.textures.size(), gpu::Texture{});
    for (size_t i = 0; i < s.textures.size(); ++i) {
        const auto& src = s.textures[i];
        if (src.width <= 0 || src.height <= 0) continue;
        auto& dst = s.es2_gpu_tex_cache[i];
        dst.width  = src.width;
        dst.height = src.height;
        dst.format = gpu::Texture::RGBA8;
        dst.filter = src.linear ? gpu::Texture::BILINEAR : gpu::Texture::NEAREST;
        dst.wrap_s = src.wrap_repeat_s ? gpu::Texture::REPEAT : gpu::Texture::CLAMP;
        dst.wrap_t = src.wrap_repeat_t ? gpu::Texture::REPEAT : gpu::Texture::CLAMP;
        dst.texels = src.texels;
    }
    for (auto& slot : s.ctx.textures) slot = nullptr;
    for (const auto& [loc, tex_slot] : baked->sampler_to_tex_slot) {
        const int unit = static_cast<int>(p.uniform_values[loc][0]);
        if (unit < 0 || unit >= (int)s.es2_tex_units.size()) continue;
        const GLuint id = s.es2_tex_units[unit];
        if (id == 0 || id >= s.es2_gpu_tex_cache.size()) continue;
        if (tex_slot < 0 || tex_slot >= (int)s.ctx.textures.size()) continue;
        s.ctx.textures[tex_slot] = &s.es2_gpu_tex_cache[id];
    }

    // Sprint 39 — scene capture for the SC chain. Run the VS in
    // software per vertex (cheap; same code that runs inside
    // gpu::pipeline::draw moments later) to extract clip-space pos +
    // varying[0] (the colour for our pass-through-style shaders), then
    // re-fan into a triangle list per `mode`. sc_pattern_runner
    // replays the resulting .scene through the cycle-accurate chain.
    if (glcompat::es2_scene_capture_enabled() && count >= 3) {
        // Sprint 61 — capture all VS-output varyings, not just o[1].
        // The runtime ThreadState.o is sized 8 (Sprint 58), so VS may
        // write o[1..7]. For the SC replay to match sw_ref on
        // multi-varying shaders (the 1060 fragment_ops.blend.* block,
        // every basic_shader case, etc.) we need to ship every active
        // varying through the scene. `n_vars` is the program's
        // num_varyings — the SC replay's pa_rs.varying_count uses the
        // same value when reconstructing the rasterizer feed.
        const int n_vars = std::max(1,
                                    std::min(7, baked->num_varyings));
        std::vector<gpu::Vec4f> vs_pos(count);
        std::vector<std::array<gpu::Vec4f, 7>> vs_var(count);
        auto to_v4 = [](const gpu::sim::Vec4& v) {
            return gpu::Vec4f{v[0], v[1], v[2], v[3]};
        };
        for (GLsizei v = 0; v < count; ++v) {
            gpu::sim::ThreadState t{};
            for (int i = 0; i < 32; ++i) {
                const auto& u = s.ctx.draw.uniforms[i];
                t.c[i] = gpu::sim::Vec4{{u[0], u[1], u[2], u[3]}};
            }
            const int n = baked->num_attribs > 0 ? baked->num_attribs : 8;
            for (int i = 0; i < n && i < 8; ++i) {
                const auto& a = attrib_streams[i][v];
                t.r[i] = gpu::sim::Vec4{{a[0], a[1], a[2], a[3]}};
            }
            (void)gpu::sim::execute(baked->vs_code, t);
            vs_pos[v] = to_v4(t.o[0]);
            for (int k = 0; k < n_vars; ++k)
                vs_var[v][k] = to_v4(t.o[1 + k]);
        }
        // Re-fan into TRIANGLES (sc_pattern_runner expects flat list).
        std::vector<gpu::Vec4f> tri_pos;
        std::vector<std::array<gpu::Vec4f, 7>> tri_var;
        const auto push3 = [&](GLsizei a, GLsizei b, GLsizei c) {
            tri_pos.push_back(vs_pos[a]); tri_var.push_back(vs_var[a]);
            tri_pos.push_back(vs_pos[b]); tri_var.push_back(vs_var[b]);
            tri_pos.push_back(vs_pos[c]); tri_var.push_back(vs_var[c]);
        };
        if (mode == GL_TRIANGLE_STRIP) {
            for (GLsizei i = 0; i + 2 < count; ++i) {
                if (i & 1) push3(i + 1, i, i + 2);
                else       push3(i,     i + 1, i + 2);
            }
        } else {
            for (GLsizei i = 0; i + 2 < count; i += 3) push3(i, i + 1, i + 2);
        }
        glcompat::scene_record_es2_batch(tri_pos, tri_var, n_vars);
    }

    gpu::pipeline::draw(s.ctx, (uint32_t)count);
}

}  // namespace

extern "C" void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (first != 0) {
        // Convert to indexed for the offset case.
        std::vector<uint32_t> idx(count);
        for (GLsizei i = 0; i < count; ++i) idx[i] = (uint32_t)(first + i);
        run_draw(mode, count, idx.data());
    } else {
        run_draw(mode, count, nullptr);
    }
}
extern "C" void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    auto& s = state();
    const uint8_t* idx_base = nullptr;
    if (s.es2_element_array_buffer
        && s.es2_element_array_buffer < s.es2_buffers.size()) {
        idx_base = s.es2_buffers[s.es2_element_array_buffer].data.data()
                 + (size_t)(uintptr_t)indices;
    } else {
        idx_base = reinterpret_cast<const uint8_t*>(indices);
    }
    std::vector<uint32_t> idx(count);
    if (type == GL_UNSIGNED_SHORT) {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(idx_base);
        for (GLsizei i = 0; i < count; ++i) idx[i] = p[i];
    } else if (type == GL_UNSIGNED_INT) {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(idx_base);
        for (GLsizei i = 0; i < count; ++i) idx[i] = p[i];
    } else {       // GL_UNSIGNED_BYTE or anything else
        const uint8_t* p = idx_base;
        for (GLsizei i = 0; i < count; ++i) idx[i] = p[i];
    }
    run_draw(mode, count, idx.data());
}

// ============================================================
// FBO / RBO — Sprint 38 real render-to-texture path.
//
// Each non-zero FBO owns a separate gpu::Framebuffer in
// `state().es2_fbo_storage`. glBindFramebuffer swaps it into
// `ctx.fb` (the active render target) via std::swap on the
// vector-backed members. When unbinding from a texture-attached FBO,
// the rendered colour is copied back into the texture's texels so
// subsequent samples see the painted content.
// ============================================================
namespace {

void copy_fb_to_attached_tex(GLuint fb_id) {
    auto& s = state();
    if (fb_id == 0 || fb_id >= s.es2_fbos.size()) return;
    const auto& fbo = s.es2_fbos[fb_id];
    if (fbo.color0.target != GL_TEXTURE_2D) return;
    if (fbo.color0.name == 0 || fbo.color0.name >= s.textures.size()) return;
    const auto& fb_storage = (s.es2_bound_fb == fb_id)
                                ? s.ctx.fb
                                : s.es2_fbo_storage[fb_id];
    if (fb_storage.width <= 0 || fb_storage.height <= 0) return;
    auto& tex = s.textures[fbo.color0.name];
    tex.width  = fb_storage.width;
    tex.height = fb_storage.height;
    tex.texels = fb_storage.color;
}

void attachment_dims(const State::Fbo& fbo, int& W, int& H) {
    auto& s = state();
    W = H = 0;
    if (fbo.color0.target == GL_TEXTURE_2D
        && fbo.color0.name < s.textures.size()) {
        W = s.textures[fbo.color0.name].width;
        H = s.textures[fbo.color0.name].height;
    } else if (fbo.color0.target == GL_RENDERBUFFER
        && fbo.color0.name < s.es2_renderbuffers.size()) {
        W = s.es2_renderbuffers[fbo.color0.name].width;
        H = s.es2_renderbuffers[fbo.color0.name].height;
    }
}

void ensure_fbo_storage(GLuint id) {
    auto& s = state();
    if (id >= s.es2_fbo_storage.size()) s.es2_fbo_storage.resize(id + 1);
    if (id == 0) return;
    auto& fb = s.es2_fbo_storage[id];
    if (fb.width > 0 && fb.height > 0) return;
    int W = 0, H = 0;
    if (id < s.es2_fbos.size()) attachment_dims(s.es2_fbos[id], W, H);
    if (W <= 0 || H <= 0) return;
    fb.width = W; fb.height = H;
    fb.color.assign((size_t)W * H, 0u);
    fb.depth.assign((size_t)W * H, 1.0f);
}

}  // namespace

extern "C" void glGenFramebuffers(GLsizei n, GLuint* out) {
    auto& s = state();
    for (GLsizei i = 0; i < n; ++i) {
        out[i] = (GLuint)s.es2_fbos.size();
        s.es2_fbos.emplace_back();
    }
}
extern "C" void glDeleteFramebuffers(GLsizei n, const GLuint* fbs) {
    for (GLsizei i = 0; i < n; ++i) {
        const GLuint id = fbs[i];
        if (id == 0 || id >= state().es2_fbos.size()) continue;
        state().es2_fbos[id] = {};
    }
}
extern "C" void glBindFramebuffer(GLenum /*target*/, GLuint id) {
    auto& s = state();
    if (id == s.es2_bound_fb) return;
    // If we're leaving a texture-attached FBO, push pixels back.
    copy_fb_to_attached_tex(s.es2_bound_fb);
    // Save current ctx.fb back to its storage slot.
    if (s.es2_bound_fb < s.es2_fbo_storage.size()) {
        std::swap(s.ctx.fb, s.es2_fbo_storage[s.es2_bound_fb]);
    }
    ensure_fbo_storage(id);
    if (id < s.es2_fbo_storage.size()) {
        std::swap(s.ctx.fb, s.es2_fbo_storage[id]);
    }
    s.es2_bound_fb = id;
    s.ctx_inited = (s.ctx.fb.width > 0);
}
extern "C" GLenum glCheckFramebufferStatus(GLenum) { return GL_FRAMEBUFFER_COMPLETE; }
extern "C" void glFramebufferTexture2D(GLenum /*target*/, GLenum attachment,
                                       GLenum textarget, GLuint tex, GLint /*level*/) {
    auto& s = state();
    if (s.es2_bound_fb >= s.es2_fbos.size()) return;
    auto& fbo = s.es2_fbos[s.es2_bound_fb];
    State::FboAttachment a{textarget, tex};
    switch (attachment) {
        case GL_COLOR_ATTACHMENT0: fbo.color0  = a; break;
        case GL_DEPTH_ATTACHMENT:  fbo.depth   = a; break;
        case GL_STENCIL_ATTACHMENT:fbo.stencil = a; break;
        default: break;
    }
}
extern "C" void glGenRenderbuffers(GLsizei n, GLuint* out) {
    auto& s = state();
    for (GLsizei i = 0; i < n; ++i) {
        out[i] = (GLuint)s.es2_renderbuffers.size();
        s.es2_renderbuffers.emplace_back();
    }
}
extern "C" void glDeleteRenderbuffers(GLsizei n, const GLuint* rbs) {
    for (GLsizei i = 0; i < n; ++i) {
        const GLuint id = rbs[i];
        if (id == 0 || id >= state().es2_renderbuffers.size()) continue;
        state().es2_renderbuffers[id] = {};
    }
}
extern "C" void glBindRenderbuffer(GLenum, GLuint) {}     // no implicit binding state used
extern "C" void glRenderbufferStorage(GLenum /*target*/, GLenum format, GLsizei w, GLsizei h) {
    // The most-recent gen'd RBO that hasn't been sized wins; no separate
    // bound-RBO state, so for simplicity we resize the highest unsized
    // entry. Real driver tracks bound-RBO; not needed for the runner.
    auto& s = state();
    for (auto it = s.es2_renderbuffers.rbegin(); it != s.es2_renderbuffers.rend(); ++it) {
        if (it->width == 0 && it->height == 0) {
            it->format = format; it->width = w; it->height = h;
            return;
        }
    }
}
extern "C" void glFramebufferRenderbuffer(GLenum /*target*/, GLenum attachment,
                                          GLenum /*rbtarget*/, GLuint rb) {
    auto& s = state();
    if (s.es2_bound_fb >= s.es2_fbos.size()) return;
    auto& fbo = s.es2_fbos[s.es2_bound_fb];
    State::FboAttachment a{GL_RENDERBUFFER, rb};
    switch (attachment) {
        case GL_COLOR_ATTACHMENT0: fbo.color0  = a; break;
        case GL_DEPTH_ATTACHMENT:  fbo.depth   = a; break;
        case GL_STENCIL_ATTACHMENT:fbo.stencil = a; break;
        default: break;
    }
}

// ============================================================
// Misc
// ============================================================
extern "C" void glClearDepthf(GLclampf d) { state().clear_depth = d; }
extern "C" void glActiveTexture(GLenum tex) { state().es2_active_texture = tex; }
extern "C" void glGenerateMipmap(GLenum)    {}      // we only sample base mip
// glReadPixels / glGetBooleanv / glIsEnabled already live in
// glcompat_state.cpp — declared in GL/gl.h above for the ES 2.0 docs
// surface, but the implementations are shared with the legacy 1.x
// path. No new definitions here.
