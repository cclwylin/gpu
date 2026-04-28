#pragma once
#include <span>
#include <vector>

#include "state.h"
#include "types.h"

namespace gpu::pipeline {

// 階段函式;每個對應 docs/microarch/<block>.md 的 SW 等價。
// Phase 1 skeleton:這些大多 stub。triangle 路徑 hook 已通,enough to render
// hello-triangle in MSAA 1x mode.

// VF stage
void vertex_fetch(Context& ctx, std::vector<std::array<Vec4f, 8>>& out_attrs,
                  uint32_t vertex_count);

// VS stage — calls bound vs function per vertex.
void vertex_shader(Context& ctx,
                   std::span<const std::array<Vec4f, 8>> in_attrs,
                   std::vector<Vertex>& out_verts);

// PA stage — perspective divide + viewport + cull + assemble.
void primitive_assembly(Context& ctx,
                        std::span<const Vertex> in_verts,
                        std::vector<Triangle>& out_tris);

// RS — coverage rasterization. Skeleton: 1× sample, top-left rule.
void rasterizer(Context& ctx,
                std::span<const Triangle> tris,
                std::vector<Quad>& out_quads);

// FS — calls bound fs function per fragment.
void fragment_shader(Context& ctx, Quad& quad);

// PFO — depth/stencil/blend; skeleton writes color directly, no blend.
void per_fragment_ops(Context& ctx, const Quad& quad);

// Resolve — 4× -> 1× box filter; skeleton no-op when msaa_4x=false.
void resolve(Context& ctx);

// Top-level convenience: run end-to-end for a draw call.
void draw(Context& ctx, uint32_t vertex_count);

}  // namespace gpu::pipeline
