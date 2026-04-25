# docs/microarch/ — Per-Block Microarchitecture

每個 block 一份 microarch 文件。draft v0.1 首版已寫完,Phase 0 內 iterate。

## Index

| Block | File | Owner |
|---|---|---|
| Command Processor | [cp.md](cp.md) | E1 |
| Vertex Fetch | [vf.md](vf.md) | E1 |
| Shader Core | [sc.md](sc.md) | E1 |
| Primitive Assembly | [pa.md](pa.md) | E1 |
| Tile Binner | [tb.md](tb.md) | E2 |
| Rasterizer | [rs.md](rs.md) | E2 |
| Texture Unit | [tmu.md](tmu.md) | E2 |
| Per-Fragment Ops | [pfo.md](pfo.md) | E2 |
| Tile Buffer | [tbf.md](tbf.md) | E2 |
| Resolve Unit | [rsv.md](rsv.md) | E2 |
| MMU | [mmu.md](mmu.md) | E1 |
| L2 Cache | [l2.md](l2.md) | E1 |
| Memory Controller | [mc.md](mc.md) | E1 |
| CSR Block | [csr.md](csr.md) | E3 |
| Perf Monitor Unit | [pmu.md](pmu.md) | E3 |

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
