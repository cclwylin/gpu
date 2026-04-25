# tools/ — Utilities

```
trace_diff/     Stage-boundary trace comparator + visualizer
scene_gen/      Programmatic scene generator
regmap_gen/     specs/registers.yaml → C header / SystemC / Verilog / doc
isa_gen/        specs/isa.yaml → assembler / decoder / sim skeleton
```

## regmap_gen
- Input:`specs/registers.yaml`
- Output:
  - `driver/include/gen/gpu_regs.h`
  - `systemc/common/gen/regs.h`
  - `rtl/blocks/csr/gen/csr_regs.sv`
  - `docs/gen/register_map.md`
- CI 強制 output 與 input 一致(no manual drift)

## isa_gen
- Input:`specs/isa.yaml`
- Output:
  - `compiler/assembler/gen/opcodes.cpp`
  - `compiler/isa_sim/gen/dispatch.cpp`
  - `systemc/common/gen/isa_decoder.h`
  - `rtl/blocks/sc/gen/decoder.sv`
  - `docs/gen/isa_reference.md`

## scene_gen
- 生成 `.scene` 檔(自訂簡易 format):state setup + draw list + expected framebuffer
- 參數化:primitive 類型、count、viewport、shader 組合、MSAA on/off

## trace_diff
- 讀 sw_ref 和 HW model 的 stage trace
- Alignment(HW 有 latency,靠 tag/id 比對)
- 視覺化 diff(html dump)
