# GPU Project — OpenGL ES 2.0 Programmable GPU (RTL Target)

一個完整的 GPU hardware 實作專案,從 OpenGL simulation → SystemC cycle-accurate
model → synthesizable RTL,支援 OpenGL ES 2.0 programmable pipeline + 4× MSAA。

## Target

| 項目 | 規格 |
|---|---|
| **API** | OpenGL ES 2.0 (full) + 4× MSAA + alpha-to-coverage |
| **Architecture** | TBDR (Tile-Based Deferred Rendering) + Unified SIMT Shader |
| **Tile Size** | 16 × 16 pixel |
| **Warp** | 16 threads (4 lane × 4 thread) |
| **Frequency** | 1 GHz @ 28nm / 500 MHz FPGA |
| **Peak** | 1 pixel/cycle (1×), ~0.8 pixel/cycle (4× MSAA) |
| **Bus** | AXI4 (2× 128-bit master) + APB (CSR) |
| **Delivery** | Synthesizable Verilog RTL, tape-out ready |
| **Timeline** | 28 months, 2–3 FTE |

## Status

**Phase 0 — Architecture Freeze** (kick-off)

## 專案結構

```
docs/            規格文件(single source of truth)
specs/           機器可讀 spec(YAML,自動生 header/SystemC/RTL/doc)
sw_ref/          C++ OpenGL ES 2.0 reference model (golden)
compiler/        GLSL → SPIR-V → IR → shader ISA
systemc/         SystemC TLM + cycle-accurate model
rtl/             Verilog RTL (Phase 3)
driver/          Linux EGL + GLES 2.0 driver
tests/           Scene corpus + shader corpus + conformance
tools/           Trace diff / scene gen / regmap gen
third_party/     SystemC / DRAMSim3 / glslang
ci/              CI scripts + coding style check
```

## 關鍵文件

- [Master Plan](docs/MASTER_PLAN.md) — 28 個月總規劃,單一真實來源
- [PROGRESS](docs/PROGRESS.md) — 已交付項、目前狀態、open follow-ups
- [Architecture Spec](docs/arch_spec.md) — 頂層 block 圖與資料流
- [ISA Spec](docs/isa_spec.md) — Shader instruction set
- [MSAA Spec](docs/msaa_spec.md) — 4× MSAA 實作細節
- [Coding Style](docs/coding_style.md) — C++/SystemC/Verilog 規範
- [Register Map](specs/registers.yaml) — CSR 定義
- [ISA YAML](specs/isa.yaml) — 機器可讀 ISA

## Quick Start

Phase 0 建置中,toolchain bring-up 完成後會補。

## Contribution

- 所有 spec 變更走 PR review
- CI 綠才能 merge
- 詳見 [`docs/coding_style.md`](docs/coding_style.md)
