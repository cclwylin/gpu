# docs/ — Specification & Documentation

所有 spec 都在這,是 single source of truth。變更走 PR review。

## Index

| 文件 | 內容 | Status |
|---|---|---|
| [MASTER_PLAN.md](MASTER_PLAN.md) | 28-month E2E plan,team、timeline、deliverables | frozen v1.0 |
| [PROGRESS.md](PROGRESS.md) | Plan-vs-actual 進度日誌(每 commit 更新) | live |
| [arch_spec.md](arch_spec.md) | 頂層 block、dataflow、memory map、interface | draft v0.1 |
| [isa_spec.md](isa_spec.md) | Shader ISA,encoding、opcode、ABI | frozen v1.0 |
| [msaa_spec.md](msaa_spec.md) | 4× MSAA 實作細節 | draft v0.1 |
| [coding_style.md](coding_style.md) | C++ / SystemC / Verilog / Python 風格 | active v1.0 |
| [microarch/](microarch/) | 每 block microarch 細節 | draft v0.1 (15 docs) |

## 機器可讀 Spec(specs/)

- [registers.yaml](../specs/registers.yaml) — CSR 定義(自動生 header/SystemC/RTL/doc)
- [isa.yaml](../specs/isa.yaml) — ISA 定義(自動生 assembler/decoder)

## 更新規則

1. 所有 spec 變更 = PR,link issue,標明 motivation
2. Spec 與 code 不同步 = 開 follow-up issue,48 小時內 close
3. Version bump:
   - `draft v0.X` → Phase 0 內 iteration
   - `frozen v1.0` → Phase 0 exit 鎖定
   - `vX.Y` → 後續修訂
