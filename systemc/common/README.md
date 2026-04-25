# systemc/common/ — Shared SystemC Infrastructure

放 block 之間共用的 types / protocols / utility。

## 內容(規畫)
```
common/
  types/            pixel, fragment, triangle, vertex, coverage mask
  tlm/              TLM-2.0 generic payload extensions (GPU-specific)
  axi/              AXI4 master/slave BFM (for external MC / testbench)
  apb/              APB BFM
  fp/               SystemC FP wrapper (shared with sw_ref/fp)
  gen/              auto-generated (regs.h from specs/registers.yaml,
                                      isa decoder from specs/isa.yaml)
  utils/            logging, assertion, scoreboard helpers
```

## Phase 0 建立最早的三個
1. `common/types/` — 所有 pipeline 資料型別(pixel/fragment/quad/triangle/vertex/mask)
2. `common/gen/regs.h` — 從 `specs/registers.yaml` 生成的 register offset constants
3. `common/fp/` — 與 `sw_ref/fp/` 共用的 FP 實作
