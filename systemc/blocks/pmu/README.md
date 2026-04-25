# PMU — Performance Monitor Unit

**Role**:64× 32-bit perf counter + trace buffer,event source 來自各 block。

## Counter
- 64 個 32-bit counter,可 saturate 或 wrap
- Per-counter event mux:指定 source block + event ID
- Via CSR 配置 / 讀值

## Event sources(範例)
- CP:packets processed
- SC:warp launched、instruction count、stall cycle
- RS:triangle raster、coverage histogram
- PFO:depth fail、stencil fail、blend count
- RSV:resolve cycles
- TBF:access、conflict、spill

## MSAA-specific(必備)
- `coverage_hist_0..4` — population count histogram(0/1/2/3/4 bit set per pixel)
- `resolve_cycle`
- `tbf_spill`
- `a2c_hit`
- `early_z_per_sample_kill`

## Trace
- On-chip circular SRAM buffer
- Dump path:on-chip SRAM → DRAM(via MC)或 off-chip pin(FPGA debug)
- Trigger:software start/stop,或 event-triggered

## Owner
E3
