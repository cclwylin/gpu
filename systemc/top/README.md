# systemc/top/ — Full-Chip Integration

頂層 instantiate 所有 block,外接 DRAMSim3,APB/AXI BFM。

## Deliverable
- `gpu_top.h / .cpp` — 所有 block 串接
- `gpu_top_tb.cpp` — 連接 driver-level API + scoreboard
- Phase 1 exit:跑 reference scene(1× + 4× MSAA)bit-exact vs sw_ref
- Phase 2 exit:cycle-accurate performance model 跑 3 benchmark

## Integration Order(Phase 1)
1. CP + CSR
2. VF + MMU(vertex fetch path 通)
3. SC(shader execution 通,用 isa_sim)
4. PA + TB(到 tile list 成)
5. RS + SC(FS)+ TMU(fragment 通)
6. PFO + TBF + RSV(到 framebuffer 通)
