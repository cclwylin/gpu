---
doc: ISA Validation Report
version: 1.0
status: ISA v1.0 frozen — all gaps resolved
last_updated: 2026-04-25
---

# ISA Validation Report — v1.0 (closed)

3 個 reference shader 在 v0.1 上撞出來的 6 個 ISA gap,**全部在 v1.0 解決**。
此報告記錄 v0.1 → v1.0 的決議與對應修正,作為 Phase 0 ISA freeze 紀錄。

## ISA v1.0 重大決定

| 領域 | v0.1 | v1.0 |
|---|---|---|
| Instruction width | 32-bit | **64-bit fixed** |
| Swizzle | 2-bit(4 pattern)| **8-bit per-component**(任意) |
| Source modifier | 未保留 | **negate + abs** 進 encoding |
| 3-operand encoding | 待定(paired 32?)| **Inline,src2 GPR-only** |
| Predicate use | 1-bit `p` flag(無語意定義)| **2-bit `pmd`**:`always` / `if p` / `if !p` |
| Predicate set | 無 | **`setp_{eq,ne,lt,le,gt,ge}`** |
| dp2 | 無 | **加入** opcode 0x04 |

## 6 Gap → 解決方式對照

### 1. 3-operand instruction encoding ✓
- v1.0 ALU 64-bit format 直接含 `src2_idx` (5b) + `src2_neg` + `src2_abs`
- src2 限定 GPR(節省 class bits)
- 用於 `mad`、`cmp`,未來擴充易加新 3-op

### 2. Source modifier(neg / abs)✓
- 每 source 各 1-bit `neg` + 1-bit `abs`
- Assembly 表記:`-src` / `|src|` / `-|src|`
- ref_shader_2 `add r2, c0, -v0` 為典型用例

### 3. Swizzle 2-bit → 8-bit ✓
- 8-bit / source,每 2-bit 控制一個輸出 component(00=x、01=y、10=z、11=w)
- 任意 pattern 可表達(`xyzw`、`yzwx`、`xxxz`、`wzyx` ...)
- Identity = 0xE4

### 4. Predicate set/use protocol ✓
- 設定:`setp_<cond> p, src0.x, src1.x` per-lane scalar comparison
- 使用:每 instruction 的 `pmd` 2-bit field(`always` / `if p` / `if !p`)
- 結構化 CF:`if_p / else / endif`,mask stack 深度 8

### 5. 缺 `dp2` ✓
- Opcode 0x04 加入 ALU 表
- ref_shader_3 多處替代原來 `dp3` 後忽略 z 的繞路寫法

### 6. Predicate 更新機制 ✓
- 6 個 `setp_*` first-class opcodes(0x18..0x1D)
- `cmp` 仍保留作為「3-operand select」(寫入 GPR,非 predicate)

## Reference Shader Status

| Shader | v0.1 .asm | v1.0 .asm | 差異 |
|---|---|---|---|
| ref_shader_1(Gouraud + tex)| ✓ | ✓ | 幾乎不變(基本 ALU/tex,無新 feature) |
| ref_shader_2(Phong)| 用 source negate workaround、3-op `mad` | ✓ | 用了 `-src` 明確語法 |
| ref_shader_3(Discard + Loop)| 大量推敲 | ✓ | 用 `setp_gt`、`dp2`、`(p) kil`、`(p) break`,程式碼大幅縮短與清晰 |

## Verification Plan(Phase 1)

1. **Round-trip**:assembler → bin → disassembler 對 3 個 shader = identity
2. **Bit-exact ISA sim**:跑出 fragment color 與 sw_ref 比對
   - 整數 / 精確 op:bit-exact
   - Transcendental:3-ULP tolerance
3. **HW co-sim**(Phase 1 後段):TLM model 結果 = ISA sim 結果

## Closing Notes

ISA v1.0 已凍結。後續若新增 opcode:
- 走 PR review
- 確認不衝突現有 opcode bits
- `tools/isa_gen` 重跑
- 至少加一個對應 reference shader 驗證表達力

下一個會碰到 ISA limit 的可能點:
- Compute / barrier(ES 2.0 範圍外,v1 不做)
- Atomic(同上)
- Subgroup ops(同上)
- 64-bit / int 運算(ES 2.0 不需,將來若上 ES 3.0 才看)
