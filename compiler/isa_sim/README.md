# compiler/isa_sim/ — ISA Simulator

Functional simulator for shader ISA。用於:
1. 驗證 compiler output
2. 作為 TLM SC block 的 shader execution engine(Phase 1 TLM)
3. 驗證 ISA 表達力(Phase 0)

## Scope
- 16-thread warp execution
- Execution mask stack(divergence)
- FP follow spec([`docs/isa_spec.md §8`](../../docs/isa_spec.md)):FTZ / RNE / NaN propagate
- Texture op:透過 callback 呼叫 TMU model
- Memory op:透過 callback 呼叫 memory model
- Trace output:per-instruction commit log

## Integration
- Phase 1 TLM 的 `systemc/blocks/sc/` 直接 instantiate 這支當 execution core
- Phase 2 CA 換成自己的 pipeline model,但 isa_sim 仍用於 tb golden
