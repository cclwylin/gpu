---
doc: Shader ISA Spec
version: 0.1 (draft)
status: in progress (Phase 0)
owner: E3
last_updated: 2026-04-25
---

# Shader ISA Spec

Unified shader ISA for VS 和 FS。32-bit fixed-width instruction,vec4 ALU
with per-component swizzle / write mask,per-lane predication。

本文件為 v0.1 draft,Phase 0 結束前凍結到 v1.0。機器可讀版見
[`specs/isa.yaml`](../specs/isa.yaml)。

## 1. Programming Model

### 1.1 Execution Model
- **SIMT**,warp = 16 thread(4 lane × 4 thread batch)
- Per-lane execution mask(divergence → masked execution)
- Structured control flow only:`if / else / endif`、`loop / endloop / break`
- **禁止**任意 jump / indirect branch

### 1.2 Thread State
每個 thread 擁有:
- 32× vec4 general purpose register(GPR)
- 1-bit execution mask(由 HW 管理)
- PC(warp 共享)

### 1.3 Warp-Shared State
- 16× vec4 constant register(per draw 由 driver 設定)
- Uniform buffer pointer(DRAM)
- Texture binding table

## 2. Register Model

| Name | Count | Width | Scope | Notes |
|---|---|---|---|---|
| `r0 – r31` | 32 | vec4 × 32-bit | per-thread | GPR |
| `c0 – c15` | 16 | vec4 × 32-bit | per-warp | Constant |
| `v0 – v7` | 8 | vec4 × 32-bit | per-thread | Varying (FS 專用) |
| `o0 – o3` | 4 | vec4 × 32-bit | per-thread | Output (position / color) |
| `p` | 1 | 1-bit | per-lane | Execution mask |
| `pc` | 1 | 32-bit | per-warp | Program counter |

特殊對應:
- `o0` VS:`gl_Position`(clip-space)
- `o0` FS:`gl_FragColor`
- VS 其他 `o1 – o3`:varying output,由 rasterizer 內插送給 FS `v0 – v7`

## 3. Instruction Encoding

固定 32-bit,格式依 opcode class 切三種 layout:

### 3.1 ALU Format
```
 31     26 25 24 23 22 20 19 18 17 12 11 10 9 4 3 2 1 0
┌────────┬──┬──┬────┬───┬─────┬───┬────┬───┬──┐
│ opcode │ s│ p│ dst│wm │ src0│sw0│ src1│sw1│ i│
│   6    │ 1│ 1│  5 │ 4 │  6  │ 2 │  6  │ 2 │ 5│
└────────┴──┴──┴────┴───┴─────┴───┴────┴───┴──┘
```
| Field | Bits | 說明 |
|---|---|---|
| opcode | 6 | Class + op(見 §4) |
| s | 1 | Saturate result to [0,1] |
| p | 1 | Use predicate(1 = masked) |
| dst | 5 | Destination GPR(r0–r31) |
| wm | 4 | Write mask(xyzw,bit-per-component) |
| src0 | 6 | Source reg(5-bit reg + 1-bit class r/c) |
| sw0 | 2 | Swizzle pattern(00=xyzw,01=xxxx,10=yyyy,11=zzzw,其餘擴充) |
| src1 | 6 | 同 src0 |
| sw1 | 2 | 同 sw0 |
| i | 5 | Immediate / modifier(abs/neg flag + 3-bit imm) |

三運算元 op(`mad` / `cmp`):由 opcode 指示從後續 instruction 半位元擴展,
或改用 64-bit encoding(Phase 0 決定)。**TBD**。

### 3.2 Flow Control Format
```
 31     26 25 24 23                               0
┌────────┬──┬──┬──────────────────────────────────┐
│ opcode │ c│ a│           immediate (24-bit)     │
└────────┴──┴──┴──────────────────────────────────┘
```
- `c`:condition flag(all / any / none of execution mask)
- `a`:absolute / PC-relative

### 3.3 Memory / Texture Format
```
 31     26 25 23 22 18 17 14 13 9 8 6 5 0
┌────────┬─────┬──────┬─────┬────┬───┬─────┐
│ opcode │ wm  │ dst  │ src │ tex│ md│ pad │
└────────┴─────┴──────┴─────┴────┴───┴─────┘
```
- `tex`:texture binding slot(0–15)
- `md`:mode(bias / LOD / gradient / plain)

## 4. Opcode Table(v0.1)

### 4.1 ALU(6-bit opcode,class 00xxxx)

| Op | Mnemonic | Semantics |
|---|---|---|
| 00_0000 | `nop` | no operation |
| 00_0001 | `mov` | dst = src0 |
| 00_0010 | `add` | dst = src0 + src1 |
| 00_0011 | `mul` | dst = src0 * src1 |
| 00_0100 | `mad` | dst = src0 * src1 + src2 (3-operand) |
| 00_0101 | `dp3` | dst.xyzw = dot3(src0.xyz, src1.xyz) (broadcast) |
| 00_0110 | `dp4` | dst.xyzw = dot4(src0, src1) |
| 00_0111 | `rcp` | dst.xyzw = 1/src0.x |
| 00_1000 | `rsq` | dst.xyzw = 1/sqrt(src0.x) |
| 00_1001 | `exp` | dst.xyzw = 2^src0.x |
| 00_1010 | `log` | dst.xyzw = log2(src0.x) |
| 00_1011 | `sin` | dst.xyzw = sin(src0.x) |
| 00_1100 | `cos` | dst.xyzw = cos(src0.x) |
| 00_1101 | `min` | dst = min(src0, src1) |
| 00_1110 | `max` | dst = max(src0, src1) |
| 00_1111 | `abs` | dst = abs(src0) |
| 01_0000 | `frc` | dst = src0 - floor(src0) |
| 01_0001 | `flr` | dst = floor(src0) |
| 01_0010 | `cmp` | dst = src0 >= 0 ? src1 : src2 |

Saturate bit 適用所有 ALU。

### 4.2 Flow Control(class 10xxxx)

| Op | Mnemonic | Semantics |
|---|---|---|
| 10_0000 | `bra` | PC += imm(conditional) |
| 10_0001 | `call` | push PC; PC += imm |
| 10_0010 | `ret` | pop PC |
| 10_0011 | `loop` | push loop count + PC |
| 10_0100 | `endloop` | dec count; branch |
| 10_0101 | `break` | exit innermost loop |
| 10_0110 | `kil` | discard pixel(set lane mask=0 in FS) |

### 4.3 Memory(class 11_00xx)

| Op | Mnemonic | Semantics |
|---|---|---|
| 11_0000 | `ld` | dst = mem[src0 + imm] |
| 11_0001 | `st` | mem[src0 + imm] = src1 |

### 4.4 Texture(class 11_01xx)

| Op | Mnemonic | Semantics |
|---|---|---|
| 11_0100 | `tex` | dst = tex2D(binding, src0.xy) |
| 11_0101 | `texb` | `tex` with explicit bias(src0.w) |
| 11_0110 | `texl` | `tex` with explicit LOD(src0.w) |
| 11_0111 | `texg` | `tex` with explicit gradient(src0.xy dFdx, src1.xy dFdy) |

## 5. Predication 與 Divergence

### 5.1 Execution Mask
- Warp 有 16 bit mask,每 bit 控制一個 thread。
- Divergent branch:HW 維護 mask stack(深度 = 8,巢狀限制),
  進分支 push 原 mask,分支間切換 mask,endif 時 pop。

### 5.2 Predicated Instruction
- 若 instruction 的 `p = 1`,該 instruction 只作用在 mask=1 的 lane。
- `kil` 直接把該 lane 的 mask 設為 0(永久,該 thread 剩下指令全跳過)。

### 5.3 Loop / Break
- `loop n` push 計數器。
- `break` 把 loop 內剩餘 thread 的 mask 設 0,觸發提前結束。
- `endloop` 檢查是否所有 thread 都已結束。

## 6. ABI(Application Binary Interface)

### 6.1 Varying Layout
- FS `v0 – v7` 依 VS `o1 – o7` 線性對應(`o0` 保留給 position)。
- Packing 規則:緊湊,每 varying 佔 vec4 slot,不拆分。

### 6.2 Uniform Layout
- `c0 – c15` 由 driver 透過 CSR 或 uniform buffer push。
- Uniform buffer:DRAM resident,用 `ld` 讀取。

### 6.3 Texture Binding
- 最多 16 個 binding slot(`tex 0 – 15`)。
- Binding 資訊(base addr、format、wrap、filter)存 CSR。

### 6.4 Scratch Space
- 若 register spill:driver 分配 scratch buffer,compiler 用 `ld/st` 存取。
- 每 thread scratch size 由 compiler 告知 HW(register map)。

### 6.5 Entry Point
- Shader binary 格式:`[header][text][constants][metadata]`
- Header:entry PC、GPR count、scratch size、varying/uniform count。

## 7. Assembly Syntax(draft)

```
; comment
; dst.writemask = op src0.swizzle, src1.swizzle

mul r1.xyz,  r0.xyz, c0.xxx      ; r1.xyz = r0.xyz * c0.x
mad r2,      r1,     c1,  c2      ; r2 = r1*c1 + c2
dp4 r3.x,    r2,     c3            ; r3.x = dot4(r2, c3)
tex r4,      v0.xy,  tex0          ; r4 = tex2D(tex0, v0.xy)
mov o0,      r4                    ; gl_FragColor = r4

if_ne r5.x, c4.x
    mov r6, c5
else
    mov r6, c6
endif
```

## 8. Floating Point

- Format:IEEE 754 binary32(single precision)
- Subnormal:**flush-to-zero**(input 與 output 都 FTZ)
- Rounding:round-to-nearest-even(RNE)
- NaN:propagate(不做 signaling)
- Transcendental(rcp/rsq/exp/log/sin/cos):**3 ULP** 容忍
- 精確 op(add/mul/mad):**0.5 ULP**(正確 round)

## 9. Open Questions(Phase 0 要解)

- [ ] 3-operand ALU(`mad`、`cmp`)的 encoding:64-bit 或 paired-32
- [ ] Swizzle 2-bit 是否夠表達(考慮 `xyzw` 4 個常用 pattern + 可否只支援 replicated)
- [ ] Loop stack depth 8 是否夠
- [ ] `kil` 在 VS 是否定義為 nop
- [ ] Dual-issue(scalar + vec4)是否 v1 納入

## 10. Validation Plan

Phase 0 exit 前要完成:
1. 手寫 3 個 reference shader 驗證 ISA 表達力:
   - Gouraud VS + textured FS
   - Phong VS + Phong FS(with sin/cos lighting)
   - Branching shader(`if/else/loop`)
2. Assembler + disassembler round-trip 成功
3. ISA simulator 執行結果 = ref model(bit-exact,modulo FP)

## 11. References

- [`specs/isa.yaml`](../specs/isa.yaml)
- [`MASTER_PLAN.md`](MASTER_PLAN.md)
- OpenGL ES Shading Language Spec 1.00
- SPIR-V 1.0 spec
