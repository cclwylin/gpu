# driver/ — Linux EGL + GLES 2.0 Driver

## Scope
- EGL(context / surface / display)
- GLES 2.0 API implementation
- Command buffer builder(ring buffer writer)
- Shader compile(呼叫 `compiler/` toolchain)
- Resource management(texture upload、buffer alloc、MMU page table)
- FBO + MSAA path
- Resolve path(implicit on tile flush,explicit via `glBlitFramebuffer`)

## Target
- Linux userspace(initially LibDRM-less,直接用 FPGA PCIe BAR)
- 後期考慮 Mesa 整合

## Phase Plan
- Phase 1:skeleton + command buffer writer(給 SystemC tb 用)
- Phase 2:complete state tracker + shader binding
- Phase 3+:conformance、real app support
- Phase 4:FPGA bring-up + dEQP

## Owner
E3
