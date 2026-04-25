---
doc: ISA Validation Report
version: 0.1
status: Phase 0 ISA expressiveness check
last_updated: 2026-04-25
---

# ISA Validation Report — v0.1 draft

手寫 3 個 reference shader 壓測 ISA,找出 v0.1 spec 的落差,作為 Phase 0
ISA freeze 前的修正依據。

## 覆蓋面

| Feature | ref_1 | ref_2 | ref_3 |
|---|---|---|---|
| Vec4 ALU + swizzle + wmask | ✓ | ✓ | ✓ |
| `dp3` / `dp4` | ✓ | ✓ | ✓ |
| `mul` / `add` / `mad` | ✓ | ✓ | ✓ |
| `rcp` / `rsq` | — | ✓ | ✓ |
| `exp` / `log`(pow lowering) | — | ✓ | — |
| `max` / `min` / `cmp` | — | ✓ | ✓ |
| Texture `tex` | ✓ | — | — |
| Varying input / output | ✓ | ✓ | ✓ |
| Predication | — | — | ✓ |
| `loop` / `endloop` / `break` | — | — | ✓ |
| `kil` | — | — | ✓ |
| Source negate modifier | — | ✓ | ✓ |

## 發現的 ISA 落差(Phase 0 freeze 前需決定)

### 1. 3-operand instruction encoding(`mad`、`cmp`)
**問題**:v0.1 spec §3.1 ALU format 只有 2 source field。shader 2 / 3 大量使用 `mad`、
`cmp` 的 3-operand 形式。

**選項**:
- **A. 64-bit encoding for 3-op**(變長指令)
- **B. 隱式 src2 = dst**(等價於 `dst = src0*src1 + dst`,限制彈性)
- **C. 保留 opcode 指示下一 word 為 src2(配對 2 × 32-bit)**

**建議 C**:保持 32-bit instruction width 簡化 fetch/decode,VLIW-like pairing
少見但可管理。

### 2. Source modifier(negate / abs)
**使用點**:ref_2 `add r2, c0, -v0`、ref_3 `mad r5.x, -r2.y, ...`。

**v0.1 現況**:modifier bits 在 encoding 中未明確保留。

**決定**:ALU format 的 `i` 5-bit field 拆成:
- bit 0:src0 negate
- bit 1:src0 abs
- bit 2:src1 negate
- bit 3:src1 abs
- bit 4:saturate(已有 `s` bit,此 bit 另用)
  → 此處需收緊規劃,Phase 0 rework encoding

### 3. Swizzle 2-bit 不夠
**使用點**:`c1.xxxx`、`r0.yyyy`、`v0.xy`(implicit zw=0)、`v1.xyxx`、`r2.xyxx`。

**v0.1 現況**:2-bit swizzle 只能表達 4 種 pattern。

**shader 3 用到的 unique swizzle**:`.xyxx`、`.xxxx`、`.yyyy`、`.zzzz`、`.wwww`、`.xy`(partial)。
至少要 **full 4×2-bit(8 bit)** 才能表達 arbitrary per-component swizzle。

**建議**:**擴 swizzle 到 8-bit**(`xyzw` 每 component 2 bit = 4 pick)。
Encoding 需重算。若 32-bit 不夠:
- 縮 source reg(6-bit → 5-bit,限制 constant 與 GPR 分開取)
- 或直接上 64-bit indirect mode

### 4. Predicated execution encoding
**ref_3 用法**:`cmp p, ..., ...`(寫入 predicate reg);`(p) break`(以 p 作為條件執行)。

**v0.1 現況**:`p` bit 存在,但:
- 如何「設定」predicate(哪個 instruction 產生 p)
- 如何「使用」predicate(非 flow control 的普通 ALU 是否也支援)

**建議**:
- `cmp.p dst, src0, src1, src2`:`cmp` 多一個 flag 把結果同時寫 dst 和 predicate
- Flow control instruction 支援 `(p)` prefix 為 conditional

### 5. `dp2`(vec2 dot)缺
**使用點**:shader 3 的 `dot(p, p)` 是 vec2,用 `dp3` 但忽略 z。

**v0.1 現況**:沒 `dp2`。workaround 是 mov z=0 或用 mul+add。

**建議**:**加 `dp2`**,省 1 指令。低成本擴充。

### 6. Predicate update 的 syntax
Assembly 假設 `cmp p, src0, src1, src2` 直接寫 predicate;但 v0.1 ISA 的 `cmp`
是寫 GPR。**需要定義新 opcode** `setp_eq / setp_lt / setp_ge`(predicate set ops),
或將 `cmp` 擴充 flag 讓結果可同時寫 GPR 與 predicate。

## ISA v1.0 修正清單

針對 Phase 0 exit 前要 freeze 到 v1.0:

- [ ] **Encoding rework**:32-bit 保留,但重排 field 以支援 8-bit swizzle + source modifier
- [ ] **3-operand** 走 2-inst paired(方案 C),32-bit 不變長
- [ ] **Add** `dp2`、`setp_{eq,lt,ge}` opcodes
- [ ] **Predicate protocol**:明確定義 set / use 語法
- [ ] **Source modifier**:negate、abs 正式進 encoding
- [ ] **Swizzle**:擴 8-bit arbitrary

## 下一步

1. E1 + E3 對以上 6 點開 issue,Phase 0 內 resolve
2. `specs/isa.yaml` 與 `docs/isa_spec.md` 同步更新
3. `tools/isa_gen` 重跑,確保 generator 仍能產生 header
4. 這 3 個 shader 的 `.asm` 用 v1.0 syntax 改寫,作為 canonical regression

## Verification metadata

所有 reference shader 的 ISA simulator output 會放:
- `tests/shader_corpus/ref_shader_{1,2,3}/expected.trace`(Phase 1 產出)

對比 `sw_ref` 的 GLSL interpreter 結果,tolerance 依 [`isa_spec.md §8`](../../docs/isa_spec.md):
- Bit-exact(整數 / 精確 op)
- 3 ULP(transcendental、normalize 後的結果)
