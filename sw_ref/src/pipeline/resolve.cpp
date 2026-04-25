#include "gpu/pipeline.h"

namespace gpu::pipeline {

// Phase 1 skeleton: no-op when MSAA is off (PFO already wrote final FB).
// Real implementation will read color_samples[] and box-filter into color[].
void resolve(Context& ctx) {
    if (!ctx.fb.msaa_4x) return;
    // TODO(Phase 1): box filter 4 samples -> 1 pixel.
    //
    // for each pixel:
    //   sum = s0 + s1 + s2 + s3
    //   color = (sum + 2) >> 2
}

}  // namespace gpu::pipeline
