#include "gpu/state.h"

// GL API entry-point skeletons. Phase 1 will fill these out:
//   glClear, glClearColor, glViewport, glEnable, glDisable, glDrawArrays, ...
//
// We deliberately don't expose OpenGL/EGL headers here; sw_ref drives the
// pipeline via the gpu::Context API directly, and the real driver in driver/
// will translate gl* -> Context state.
namespace gpu::gl_api {
[[maybe_unused]] inline void _gl_anchor() {}
}
