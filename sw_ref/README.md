# sw_ref/ — OpenGL ES 2.0 Software Reference Model

C++ golden model,整個專案之後所有 HW 都 match 它。

## Scope
- OpenGL ES 2.0 state machine + entry points
- Full graphics pipeline(VF → VS → PA → RS → FS → PFO → Resolve)
- Per-sample raster / depth / stencil(4× MSAA-aware)
- Alpha-to-coverage
- FP 實作與 HW 目標 bit-accurate(IEEE754 binary32,FTZ,RNE)

## 目錄結構(規畫)
```
sw_ref/
  gl_api/                # gl*() entry points, state
  pipeline/
    vertex_fetch.cpp
    vertex_shader.cpp    # GLSL interpreter (Phase 1 early)
    primitive_assembly.cpp
    rasterizer.cpp       # coverage-aware, 4× sample
    fragment_shader.cpp
    per_fragment_ops.cpp
    resolve.cpp
  fp/                    # IEEE754 subset impl shared with HW
  dump/                  # Stage-boundary trace writer
  include/
```

## Deliverable
- Phase 1 exit:ES 2.0 CTS subset + MSAA tests 全綠。
- Stage-boundary trace 格式定稿,供 Phase 1/2 HW co-sim 比對。

## Owner
E3

## References
- [docs/arch_spec.md](../docs/arch_spec.md)
- [docs/msaa_spec.md](../docs/msaa_spec.md)
