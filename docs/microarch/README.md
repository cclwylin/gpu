# docs/microarch/ — Per-Block Microarchitecture

每份 microarch 文件對應 `systemc/blocks/<name>/` 同名 block。
File names are lowercase concatenated full names (no abbreviations) to
match the systemc/blocks/ convention.

## Index

| Block | File | Owner |
|---|---|---|
| Command Processor | [commandprocessor.md](commandprocessor.md) | E1 |
| Vertex Fetch | [vertexfetch.md](vertexfetch.md) | E1 |
| Shader Core | [shadercore.md](shadercore.md) | E1 |
| Primitive Assembly | [primitiveassembly.md](primitiveassembly.md) | E1 |
| Tile Binner | [tilebinner.md](tilebinner.md) | E2 |
| Rasterizer | [rasterizer.md](rasterizer.md) | E2 |
| Texture Unit | [textureunit.md](textureunit.md) | E2 |
| Per-Fragment Ops | [perfragmentops.md](perfragmentops.md) | E2 |
| Tile Buffer | [tilebuffer.md](tilebuffer.md) | E2 |
| Resolve Unit | [resolveunit.md](resolveunit.md) | E2 |
| Memory Management Unit | [memorymanagementunit.md](memorymanagementunit.md) | E1 |
| L2 Cache | [l2cache.md](l2cache.md) | E1 |
| Memory Controller | [memorycontroller.md](memorycontroller.md) | E1 |
| Control/Status Register | [controlstatusregister.md](controlstatusregister.md) | E3 |
| Perf Monitor Unit | [perfmonitorunit.md](perfmonitorunit.md) | E3 |

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
- `frozen v1.0`:Phase 0 exit 鎖定
