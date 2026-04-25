#include "gpu/pipeline.h"

#include <cassert>
#include <cstring>

namespace gpu::pipeline {

void vertex_fetch(Context& ctx, std::vector<std::array<Vec4f, 8>>& out_attrs,
                  uint32_t vertex_count) {
    out_attrs.assign(vertex_count, {});
    for (size_t slot = 0; slot < ctx.attribs.size(); ++slot) {
        const auto& a = ctx.attribs[slot];
        if (!a.enabled || a.data == nullptr) continue;

        const auto* base = static_cast<const uint8_t*>(a.data);
        for (uint32_t i = 0; i < vertex_count; ++i) {
            Vec4f v{};
            const uint8_t* p = base + a.stride * i + a.offset;
            switch (a.format) {
                case VertexAttribBinding::F32: {
                    float tmp[4] = {0, 0, 0, 1};
                    std::memcpy(tmp, p, sizeof(float) * a.component_count);
                    v[0] = tmp[0]; v[1] = tmp[1]; v[2] = tmp[2]; v[3] = tmp[3];
                    break;
                }
                case VertexAttribBinding::U8N: {
                    for (uint8_t k = 0; k < a.component_count; ++k) {
                        v[k] = static_cast<float>(p[k]) / 255.0f;
                    }
                    if (a.component_count < 4) v[3] = 1.0f;
                    break;
                }
                case VertexAttribBinding::U16N: {
                    const uint16_t* q = reinterpret_cast<const uint16_t*>(p);
                    for (uint8_t k = 0; k < a.component_count; ++k) {
                        v[k] = static_cast<float>(q[k]) / 65535.0f;
                    }
                    if (a.component_count < 4) v[3] = 1.0f;
                    break;
                }
            }
            out_attrs[i][slot] = v;
        }
    }
}

}  // namespace gpu::pipeline
