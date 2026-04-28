# sw_ref/ — OpenGL ES 2.0 Software Reference Model

C++20 golden model — 整個專案之後所有 HW 都 match 它。

## Status

**Phase 1 skeleton(M2.5 起)**:已 build 通、CTest 過、smoke 畫得出三角形。
端到端路徑(VF → VS → PA → RS → FS → PFO)已通,但每一站都是最小骨架。
Phase 1 會把每個 stage 補完整。

## Scope(終極目標)

- OpenGL ES 2.0 state machine + entry points
- Full graphics pipeline(VF → VS → PA → RS → FS → PFO → Resolve)
- Per-sample raster / depth / stencil(4× MSAA-aware)
- Alpha-to-coverage
- FP 實作與 HW 目標 bit-accurate(IEEE754 binary32, FTZ, RNE)

## 目錄

```
sw_ref/
  include/gpu/        public headers (types, state, pipeline, fp, trace)
  src/
    fp/               IEEE754 helpers (shared with HW)
    types/
    state/
    pipeline/         VF, VS, PA, RS, FS, PFO, Resolve
    trace/            stage-boundary trace writer
    gl_api/           gl* entry points (skeleton)
    main_smoke.cpp    hello-triangle executable
  tests/              CTest unit tests
  CMakeLists.txt
```

## Build & Test

從 repo root:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
```

Smoke executable(寫出 PPM):
```
SMOKE_OUT=/tmp/smoke.ppm ./build/sw_ref/sw_ref_smoke
```

預期 64×64 framebuffer 中三角形覆蓋 ~1300 pixel。

## What 還沒做(Phase 1 工作項)

| 元件 | 現況 | 目標 |
|---|---|---|
| GLSL interpreter | C++ functor stub | 接 SPIR-V → IR → eval |
| Stage-boundary trace | header + writer 框架 | 每 stage 實際 emit + tool diff |
| MSAA path | resolve no-op | per-sample raster + box filter |
| Depth / stencil / blend | not in PFO | 完整 ES 2.0 |
| Texture | 沒接 | RGBA8 / RGB565 / ETC1 + filter |
| GL API surface | gl_api/ stub | 完整 ES 2.0 entry points |
| FP HW alignment | 用 libm | 自實作 3-ULP 容忍的 transcendental |
| CTS | 未跑 | ES 2.0 CTS subset 全綠 |

## Phase 1 Exit Criteria

- ES 2.0 CTS subset + MSAA tests 全綠
- 跑 reference scene 與 systemc TLM bit-exact(via stage trace diff)

## Owner

E3
