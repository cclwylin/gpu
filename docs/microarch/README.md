# docs/microarch/ — Per-Block Microarchitecture

每個 block 一份 microarch 文件,Phase 0 內逐步補齊。

## Template

建議結構:
1. Purpose
2. Block diagram
3. Interface(ports + protocols)
4. Internal pipeline / FSM
5. Register file / state
6. Timing / throughput target
7. Corner cases
8. Verification plan
9. Open questions

## Planned Files

| File | Block | Owner | Target |
|---|---|---|---|
| `cp.md` | Command Processor | E1 | Phase 0 end |
| `vf.md` | Vertex Fetch | E1 | Phase 0 end |
| `sc.md` | Shader Core | E1 | Phase 0 end |
| `pa.md` | Primitive Assembly | E1 | Phase 0 end |
| `tb.md` | Tile Binner | E2 | Phase 0 end |
| `rs.md` | Rasterizer | E2 | Phase 0 end |
| `tmu.md` | Texture Unit | E2 | Phase 0 end |
| `pfo.md` | Per-Fragment Ops | E2 | Phase 0 end |
| `tbf.md` | Tile Buffer | E2 | Phase 0 end |
| `rsv.md` | Resolve Unit | E2 | Phase 0 end |
| `mmu.md` | MMU | E1 | Phase 0 end |
| `l2.md` | L2 Cache | E1 | Phase 0 end |
| `mc.md` | Memory Controller | E1 | Phase 0 end |
| `csr.md` | CSR Block | E3 | Phase 0 end |
| `pmu.md` | Perf Monitor Unit | E3 | Phase 0 end |
