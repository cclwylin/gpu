# docs/microarch/ — Per-Block Microarchitecture

每份 microarch 文件對應 `systemc/blocks/<name>/` 同名 block。
File names are lowercase concatenated full names (no abbreviations) to
match the systemc/blocks/ convention.

## Index (all v1.0 frozen as of 2026-04-26)

| Block | File | Owner | Version |
|---|---|---|---|
| Command Processor | [commandprocessor.md](commandprocessor.md) | E1 | 1.0 frozen |
| Vertex Fetch | [vertexfetch.md](vertexfetch.md) | E1 | 1.0 frozen |
| Shader Core | [shadercore.md](shadercore.md) | E1 | 1.0 frozen |
| Primitive Assembly | [primitiveassembly.md](primitiveassembly.md) | E1 | 1.0 frozen |
| Tile Binner | [tilebinner.md](tilebinner.md) | E2 | 1.0 frozen |
| Rasterizer | [rasterizer.md](rasterizer.md) | E2 | 1.0 frozen |
| Texture Unit | [textureunit.md](textureunit.md) | E2 | 1.0 frozen |
| Per-Fragment Ops | [perfragmentops.md](perfragmentops.md) | E2 | 1.0 frozen |
| Tile Buffer | [tilebuffer.md](tilebuffer.md) | E2 | 1.0 frozen |
| Resolve Unit | [resolveunit.md](resolveunit.md) | E2 | 1.0 frozen |
| Memory Management Unit | [memorymanagementunit.md](memorymanagementunit.md) | E1 | 1.0 frozen |
| L2 Cache | [l2cache.md](l2cache.md) | E1 | 1.0 frozen |
| Memory Controller | [memorycontroller.md](memorycontroller.md) | E1 | 1.0 frozen |
| Control/Status Register | [controlstatusregister.md](controlstatusregister.md) | E3 | 1.0 frozen |
| Perf Monitor Unit | [perfmonitorunit.md](perfmonitorunit.md) | E3 | 1.0 frozen |

每份新增「Implementation Status」段落,標註對應的 LT/CA SystemC class
與 sw_ref 入口、以及哪個 Sprint 出貨。Open Questions 中已決議者標 `[x]`
並寫上決議,Phase 2.x 留作 follow-up 者保留 `[ ]` 並備註理由。

## Template

每份結構:
1. Purpose
2. Block Diagram
3. Interface
4. Internal pipeline / FSM / state
5. Timing / throughput target
6. Corner cases
7. Verification plan
8. Open questions

## Status Convention

- `draft v0.1`:首版,待 Phase 0 review
- `draft v0.X`:Phase 0 內 iterate
- `frozen v1.0`:Phase 0 exit 鎖定 — **所有 15 份目前在此狀態(Sprint 35)**

進一步演進(microarch v1.1+)由 Phase 2.x 工作 driven:
real ring-fetch、warp scheduler、cache hierarchy、bank conflict modeling、
event/trace bus 等,落地時就地更新對應 doc 並 bump minor version。
