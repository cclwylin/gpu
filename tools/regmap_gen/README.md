# regmap_gen

從 [`specs/registers.yaml`](../../specs/registers.yaml) 自動生成:

- `driver/include/gen/gpu_regs.h` — C header
- `systemc/common/gen/regs.h` — SystemC constexpr header
- `rtl/blocks/csr/gen/csr_regs.svh` — SystemVerilog header
- `docs/gen/register_map.md` — rendered markdown

## Usage

```bash
python3 tools/regmap_gen/regmap_gen.py
```

CI 會跑此腳本後 `git diff --exit-code`,檢查 generated 是否與 spec 同步。

## Idempotent
同樣輸入 → 同樣輸出,bit-by-bit。Write-if-changed,避免 mtime churn。
