---
doc: Coding Style Guide
version: 1.0
status: active
owners: all
last_updated: 2026-04-25
---

# Coding Style Guide

統一 C++、SystemC、Verilog、Python 風格。CI 會檢查,違反不過。

## 1. 通則

### 1.1 Naming
- C++ / SystemC 檔名:`snake_case.cpp / .h`
- Verilog 檔名:`snake_case.v / .sv`
- Python 檔名:`snake_case.py`
- Class / Type:`UpperCamelCase`
- 函式 / 變數:`snake_case`
- 常數 / macro:`UPPER_SNAKE_CASE`
- Block 模組名:依 [Master Plan A9](MASTER_PLAN.md#a9-頂層-block-清單) 之縮寫(`cp`、`sc`、…)

### 1.2 Indent / Line
- C++ / Verilog / Python:4 space,**no tab**
- Line width:100 chars(hard limit 120)
- 檔尾留空行

### 1.3 File Header
每個原始檔必須有 header:
```
// SPDX-License-Identifier: (TBD)
// Block:  <block-name>
// File:   <filename>
// Brief:  <one-line>
```

### 1.4 Commit Message
```
<area>: <short subject>

<body: 為什麼,不是做了什麼>

Refs: #<issue>
```
`<area>` 範例:`arch`、`sw-ref`、`compiler`、`tlm`、`ca`、`rtl`、`tb`、`driver`、`ci`、`docs`。

### 1.5 PR Rules
- PR 必須 link issue / 對應 spec 章節
- 至少 1 reviewer approve
- CI 綠才能 merge
- Spec 變更必須同時更新 affected code(或開 follow-up issue)
- Squash-merge 預設

## 2. C++ / SystemC Ref / Models

### 2.1 Standard
- C++17,libstdc++ / libc++ 均可
- 禁用 RTTI / exceptions in hot path(allowed in tools)

### 2.2 Headers
- 所有 header 有 `#pragma once`
- Include order:own header → system → third_party → project
- Forward-declare 優先於 include

### 2.3 SystemC 特別規則(為 synthesis-friendly)

**Phase 2 起,systemc/blocks/** 內的 DUT 模組遵守 synthesizable subset:
- **允許**:`sc_module`、`sc_signal`、`sc_in/out`、`SC_METHOD` / `SC_CTHREAD`、
  `sc_uint/sc_int/sc_bv/sc_logic`、固定長陣列
- **禁止**(in DUT):
  - `SC_THREAD`(用 `SC_CTHREAD` 搭 clock)
  - `std::vector` / `std::map` / dynamic allocation
  - `printf` / `cout`(用 `SC_REPORT_*`,且 `ifdef DEBUG`)
  - 浮點型別(用定點或 int)
  - `sc_fifo` in RTL path(OK 在 testbench)
- Testbench 可用完整 C++/STL
- 所有 DUT module 標 `// synth-friendly` 於 file header

### 2.4 Lint Tools
- `clang-tidy`(設定檔 `.clang-tidy` in root)
- `clang-format`(設定檔 `.clang-format` in root)
- CI 檢 `clang-format --dry-run --Werror`

## 3. Shader Compiler

### 3.1 IR Design Rules
- SSA form 強制
- 每 IR node 有 source location(回溯 GLSL / SPIR-V)
- Type system 強制匹配(vec4/vec3/vec2/float/int/bool)

### 3.2 Pass Structure
- 每 pass 一個檔案
- Pass 間通信透過 IR attribute,不用全域狀態
- Pass 有 `verify()` hook,CI debug build 強制跑

## 4. Verilog / SystemVerilog RTL

### 4.1 Dialect
- Verilog 2005 或 SystemVerilog(僅 synthesizable subset)
- Testbench 可用完整 SystemVerilog + UVM

### 4.2 Scan-Ready Rules(RTL DUT)
- **禁止** latch(always block 必須覆蓋所有 case)
- **禁止** combinational loop
- **禁止** gated clock in RTL(clock-gate cell 由 synth 插)
- **禁止** `initial` in RTL(OK in testbench)
- 所有 FF 有明確 reset(sync or async,全 chip 統一)

### 4.3 Style
- `always_ff @(posedge clk or negedge rst_n)` 純 sequential
- `always_comb` 純 combinational(用 SV;Verilog 用 `always @*`)
- 不在同一 `always` 混合 seq + comb
- Reset polarity:active-low(`rst_n`)
- Signal naming:
  - `*_i`:input
  - `*_o`:output
  - `*_r`:register
  - `*_c` / `*_nxt`:combinational next-state
- One `module` per file

### 4.4 Lint
- `verilator --lint-only -Wall` 必須綠
- `verible-verilog-lint` 要過
- CDC 工具後期加(single domain,但仍檢 reset deassertion)

## 5. Python(Tools / Driver / cocotb)

### 5.1 Standard
- Python 3.10+
- Type hints 強制(`mypy --strict` 過)
- `black` format,`ruff` lint

## 6. Documentation

### 6.1 Spec
- Markdown(GitHub flavored)
- 數學用 inline code 或 KaTeX(若 GitHub render)
- 圖用 Mermaid,大圖外接 `.svg` 放 `docs/img/`

### 6.2 Code Comments
- **規則**:不寫 "what",只寫 "why"(non-obvious 的才寫)
- 公開 API 必須有 doc comment(Doxygen 格式)
- Block microarch 文件放 `docs/microarch/<block>.md`

## 7. Repo / Branch

### 7.1 Branch Model
- `main`:always green,protected
- `phase{N}/<topic>`:phase-long feature branch
- `feat/<topic>`:short-lived,< 1 week
- `fix/<topic>`:bug fix

### 7.2 Tag
- `vX.Y.Z`:spec freeze(如 `v1.0.0` = Phase 0 exit)
- `phase{N}-exit`:phase 完成 tag

## 8. CI 檢查清單

每 PR 跑:
1. Clang-format check
2. Clang-tidy
3. Verilator lint
4. Python `black` + `ruff` + `mypy`
5. Build(C++ / SystemC / RTL sim)
6. Smoke test(10 scene,5 min 內)

Nightly 跑:
1. Full regression(500 shader + 50 scene)
2. Coverage report
3. Perf benchmark(FPS / cycle / BW)

## 9. 例外申請
若某規則妨礙工程,開 issue 申請例外,Arch lead + 1 peer 同意後:
- 在 affected file 加 `// lint-exception: <rule>, reason: <why>, issue: #N`
- 更新本文件
