#pragma once
#include <cstdint>

namespace gpu::fp {

// IEEE 754 binary32 helpers. We canonicalise the path so HW and SW share the
// same flush-to-zero / RNE / NaN-propagate behaviour. See docs/isa_spec.md §7.

// Flush subnormals to zero (input or output).
float ftz(float x);

// Saturate to [0, 1].
float sat(float x);

// Reciprocal / reciprocal-sqrt with HW-matching ULP tolerance (3 ULP target).
// In skeleton we just call std::sqrt etc.; Phase 1 will make this bit-aligned.
float rcp_approx(float x);
float rsq_approx(float x);
float exp2_approx(float x);
float log2_approx(float x);
float sin_approx(float x);
float cos_approx(float x);

}  // namespace gpu::fp
