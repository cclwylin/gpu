#pragma once
#include <string>

namespace gpu::glmark2_runner {

// One-time setup: registers every baked (vs,fs) the runner scenes use.
void register_baked_programs();

// Source strings — exposed so the scenes can pass them through
// glShaderSource exactly the way real glmark2 code would.
extern const std::string kPosColorVs;
extern const std::string kPosColorFs;

}  // namespace gpu::glmark2_runner
