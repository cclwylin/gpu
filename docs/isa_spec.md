---
doc: Shader ISA Spec
version: 1.1 (frozen)
status: active
owner: E3
last_updated: 2026-04-25
---

# Shader ISA Spec — v1.1

Unified shader ISA for VS / FS。**64-bit fixed-width** instruction,
vec4 ALU with full 8-bit swizzle、source modifier、per-lane predication。

機器可讀版見 [`specs/isa.yaml`](../specs/isa.yaml)(SSOT)。

## 0. Changelog

### v1.1(Sprint 7)
| 變更 | 原因 |
|---|---|
| MEM format 加 `dst_class`(1 bit)+ `src_class`(2 bit)| 移除 Sprint 4 的 `mov rN, vN; tex; mov oN, rN` workaround;`tex` / `ld` / `st` 直接支援 GPR/const/varying 為 source、GPR/output 為 dest |
| MEM `imm` 27 → 24 bit(signed) | 為上面兩 field 騰出空間,texture/memory op 24-bit offset 仍綽綽有餘 |
| Per-lane `break` 語義正式化 | breaking lane 清掉自己 active bit,loop 對其他 lane 繼續;全部 lane break 才退出 loop。取代 v1.0 simulator 的「warp-uniform」近似 |

### v1.0 vs v0.1
| 變更 | 原因 |
|---|---|
| Instruction width 32 → **64-bit** | 32-bit 不夠 swizzle + modifier + 3-operand 同居 |
| Swizzle 2-bit → **8-bit per-component** | 真實 shader 需要任意 swizzle pattern |
| 加入 source modifier(neg、abs)| 多處需要,單獨 `mov + neg` 太貴 |
| 3-operand inline encoding(mad、cmp)| 取代「2-inst paired」設計,簡化 decode |
| 新 opcode:`dp2` | vec2 dot 高頻使用 |
| 新 opcode:`setp_{eq,ne,lt,le,gt,ge}` | 寫 predicate 的 first-class 指令 |
| Predicate use 透過 2-bit `pmd` field | 任意 instruction 可條件執行 |

## 1. Programming Model

### 1.1 Execution Model
- **SIMT**,warp = 16 thread(4 lane × 4 thread batch)
- Per-lane execution mask + per-lane predicate `p`
- Structured control flow only:`if_p / else / endif`、`loop / endloop / break`
- **禁止**任意 jump / indirect branch

### 1.2 Thread State
- 32× vec4 GPR
- 共享 PC、mask、loop stack(per warp)

### 1.3 Warp-Shared State
- 16× vec4 constant register
- 16-bit predicate `p`(每 lane 1 bit)
- Mask stack(深度 8)、loop stack(深度 8)、call stack(深度 4)

## 2. Register Model

| Name | Count | Width | Scope | Source class |
|---|---|---|---|---|
| `r0 – r31` | 32 | vec4 32-bit | per-thread | `00` GPR |
| `c0 – c15` | 16 | vec4 32-bit | per-warp | `01` const |
| `v0 – v7` | 8 | vec4 32-bit | per-thread | `10` varying |
| `o0 – o3` | 4 | vec4 32-bit | per-thread | (write only) |
| `p` | 1 | 16 bit | per-warp | (predicate) |
| `pc` | 1 | 32 bit | per-warp | — |

特殊對應:
- `o0` VS:`gl_Position`(clip-space)
- `o0` FS:`gl_FragColor`
- VS `o1 – o7`:varying output → FS `v0 – v6`(`o7` 沒對應到 FS,保留)

## 3. Instruction Encoding(64-bit fixed)

### 3.1 ALU Format
```
 63    58 57  55 54 50 49 46 45 44 43 39 38 31 30 29 28 27 26 22 21 14 13 12 11 7 6 5 4 0
┌────────┬──┬───┬─────┬────┬─────┬──────┬─────┬──┬──┬─────┬──────┬─────┬──┬──┬────┬──┬──┬───┐
│ opcode │s │pmd│ dst │wmsk│ s0c │ s0idx│ sw0 │n0│a0│ s1c │ s1idx│ sw1 │n1│a1│s2id│n2│a2│res│
│   6    │1 │ 2 │  5  │ 4  │  2  │  5   │  8  │ 1│ 1│  2  │  5   │  8  │ 1│ 1│  5 │ 1│ 1│ 5 │
└────────┴──┴───┴─────┴────┴─────┴──────┴─────┴──┴──┴─────┴──────┴─────┴──┴──┴────┴──┴──┴───┘
```

| Field | 說明 |
|---|---|
| `opcode` | 6-bit opcode(見 §5) |
| `s` | Saturate to [0, 1] |
| `pmd` | Predicate mode:`00`=always、`01`=if p、`10`=if !p、`11`=reserved |
| `dst` | GPR destination(0..31) |
| `wmsk` | Write mask(bit 0..3 ↔ x..w) |
| `s0c` / `s1c` | Source class:`00`=GPR、`01`=Const、`10`=Varying、`11`=reserved |
| `s0idx` / `s1idx` | Source register index |
| `sw0` / `sw1` | 8-bit swizzle(see §3.4) |
| `n0` / `n1` | Source negate |
| `a0` / `a1` | Source absolute |
| `s2id` | 3-operand src2(GPR-only) |
| `n2` / `a2` | Src2 modifier |
| (bit 4) | `dst_class`:0=GPR(`dst[4:0]` = r0..r31)、1=output(`dst[1:0]` = o0..o3) |
| `res` | Reserved 4-bit(must be 0) |

### 3.2 Flow Control Format
```
 63    58 57 56 55 54                                          0
┌────────┬─────┬───┬──────────────────────────────────────────┐
│ opcode │ pmd │abs│              imm (55, signed)            │
└────────┴─────┴───┴──────────────────────────────────────────┘
```
- `pmd`:同 ALU(支援 conditional flow)
- `abs`:1=absolute target、0=PC-relative

### 3.3 Memory / Texture Format(v1.1)
```
 63    58 57 56 55 51 50 47 46 42 41 34 33 30 29 27 26 25 24 23  0
┌────────┬─────┬─────┬────┬─────┬─────┬─────┬─────┬───┬─────┬─────┐
│ opcode │ pmd │ dst │wmsk│ src │ swz │ tex │mode │ dc│ sc  │ imm │
│   6    │  2  │  5  │ 4  │  5  │  8  │  4  │  3  │ 1 │  2  │ 24  │
└────────┴─────┴─────┴────┴─────┴─────┴─────┴─────┴───┴─────┴─────┘
```

- `dc`(`dst_class`,1 bit):0 = GPR(`dst[4:0]` = r0..r31)、1 = output(`dst[1:0]` = o0..o3)
- `sc`(`src_class`,2 bit):`00` = GPR、`01` = const、`10` = varying
- `tex`:texture binding slot 0..15
- `mode`:0=plain / 1=bias / 2=LOD / 3=gradient(對 tex);0=ld / 1=st(對 mem)
- `imm`:24-bit signed offset(v1.1 由 27 縮)

v1.0 的 workaround(`mov r_uv, v_in; tex; mov o_n, r_dst`)在 v1.1 已不需要。

### 3.4 Swizzle Encoding(8-bit)

每個 source 8 bit,每 2 bit 控制 destination 一個 component 從哪個 source component 取:

| Bits | Output component | 取自 source |
|---|---|---|
| `[1:0]` | dst.x | 00=src.x / 01=src.y / 10=src.z / 11=src.w |
| `[3:2]` | dst.y | (同上) |
| `[5:4]` | dst.z | (同上) |
| `[7:6]` | dst.w | (同上) |

常用 pattern:
| Pattern | Hex |
|---|---|
| `xyzw`(identity)| 0xE4 |
| `xxxx` | 0x00 |
| `yyyy` | 0x55 |
| `zzzz` | 0xAA |
| `wwww` | 0xFF |
| `yzwx` | 0x39 |

## 4. Predication 與 Divergence

### 4.1 Predicate Register `p`
- 16-bit,per warp
- 每 bit 對應一個 lane

### 4.2 設定 predicate
```
setp_eq p, src0.x, src1.x        ; per lane: p[lane] = src0.x == src1.x
setp_lt p, r0.x,   c4.x
```
六種比較:`eq / ne / lt / le / gt / ge`。Source 取 `.x` component(scalar)。

### 4.3 使用 predicate(任何 instruction)
透過 `pmd` field:
```
(p)  mov r1, c0       ; 只在 p[lane]=1 的 lane 執行
(!p) mov r1, c1       ; 只在 p[lane]=0 的 lane 執行
     mov r1, c2       ; 無條件
```

`(p)` 與 `(!p)` 也可加在 flow control:
```
(p) break             ; lane-conditional break
(p) bra label
```

### 4.4 結構化 CF
```
if_p              ; push mask;new mask = old mask & p
    ...
else              ; flip masked-but-not-killed lanes
    ...
endif             ; pop mask

loop 32           ; push (count, start PC)
    ...
    setp_ge p, r0.x, c1.x
    (p) break
endloop
```

`kil` 永久關掉 lane(set mask bit = 0,該 thread 後續指令全跳過)。

## 5. Opcode List

### 5.1 ALU 2-operand
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x00 | `nop` | — |
| 0x01 | `mov` | `dst = src0` |
| 0x02 | `add` | `dst = src0 + src1` |
| 0x03 | `mul` | `dst = src0 * src1` |
| 0x04 | `dp2` | `dst.xyzw = dot2(src0.xy, src1.xy)`(broadcast) |
| 0x05 | `dp3` | `dst.xyzw = dot3(src0.xyz, src1.xyz)` |
| 0x06 | `dp4` | `dst.xyzw = dot4(src0, src1)` |
| 0x07 | `rcp` | `dst.xyzw = 1/src0.x` |
| 0x08 | `rsq` | `dst.xyzw = 1/sqrt(src0.x)` |
| 0x09 | `exp` | `dst.xyzw = 2^src0.x` |
| 0x0A | `log` | `dst.xyzw = log2(src0.x)` |
| 0x0B | `sin` | `dst.xyzw = sin(src0.x)` |
| 0x0C | `cos` | `dst.xyzw = cos(src0.x)` |
| 0x0D | `min` | `dst = min(src0, src1)` |
| 0x0E | `max` | `dst = max(src0, src1)` |
| 0x0F | `abs` | `dst = abs(src0)` |
| 0x10 | `frc` | `dst = src0 - floor(src0)` |
| 0x11 | `flr` | `dst = floor(src0)` |

### 5.2 ALU 3-operand
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x12 | `mad` | `dst = src0*src1 + src2` |
| 0x13 | `cmp` | `dst = src0 >= 0 ? src1 : src2` |

### 5.3 Predicate set
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x18 | `setp_eq` | `p = (src0.x == src1.x)` |
| 0x19 | `setp_ne` | `p = (src0.x != src1.x)` |
| 0x1A | `setp_lt` | `p = (src0.x <  src1.x)` |
| 0x1B | `setp_le` | `p = (src0.x <= src1.x)` |
| 0x1C | `setp_gt` | `p = (src0.x >  src1.x)` |
| 0x1D | `setp_ge` | `p = (src0.x >= src1.x)` |

### 5.4 Flow
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x20 | `bra` | branch(predicated by `pmd`) |
| 0x21 | `call` | push PC; branch |
| 0x22 | `ret` | pop PC |
| 0x23 | `loop` | push count + start |
| 0x24 | `endloop` | dec count; conditional re-enter |
| 0x25 | `break` | exit innermost loop(predicated) |
| 0x26 | `if_p` | push mask; mask &= p |
| 0x27 | `else` | flip non-killed masked subset |
| 0x28 | `endif` | pop mask |
| 0x29 | `kil` | permanent lane mask clear(FS only) |

### 5.5 Memory
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x30 | `ld` | `dst = mem[src + imm]` |
| 0x31 | `st` | `mem[src + imm] = dst-field` |

### 5.6 Texture
| Op | Mnemonic | Semantics |
|---|---|---|
| 0x34 | `tex` | sample 2D / cube |
| 0x35 | `texb` | with bias(`src.w`) |
| 0x36 | `texl` | with explicit LOD(`src.w`) |
| 0x37 | `texg` | with explicit gradient(`src` = dx,`src2` = dy) |

## 6. ABI

### 6.1 Varying
- VS `o0` = `gl_Position`(clip-space)
- VS `o1..o7` 線性 → FS `v0..v6`
- Packing 緊湊,每 varying 一 vec4 slot

### 6.2 Uniform
- `c0 – c15` 由 driver 透過 CSR push
- 大 uniform 走 uniform buffer(DRAM,`ld` 讀)

### 6.3 Texture Binding
- 16 slot,binding info 存 CSR(addr、format、wrap、filter,見 [`registers.yaml`](../specs/registers.yaml) bank `TEX`)

### 6.4 Scratch
- Compiler 報告 per-thread scratch size,driver 配置 DRAM
- `ld` / `st` 存取

### 6.5 Shader Binary Format
```
[header]                32 byte fixed
  magic     "GPU0"      4 byte
  version   u16
  flags     u16
  entry_pc  u32
  gpr_count u8
  scratch_size u32
  varying_in_count u8
  varying_out_count u8
  uniform_count u8
  text_size u32
  consts_size u32
  metadata_size u32
[text]                  text_size byte (instructions, 64-bit each)
[constants]             constants_size byte
[metadata]              metadata_size byte (varying layout, debug info)
```

## 7. Floating Point

| Aspect | Behavior |
|---|---|
| Format | IEEE 754 binary32 |
| Subnormal | flush-to-zero(input + output) |
| Rounding | round-to-nearest-even |
| NaN | propagate(no signaling) |
| Precise ops(add/mul/mad/dp*)| 0.5 ULP |
| Transcendental(rcp/rsq/exp/log/sin/cos)| 3 ULP |

## 8. Assembly Syntax

```
; Comments start with ;
; Instruction: [(p|!p)] op[.sat] dst[.wmask], src0[.swiz][modifier], src1, [src2]

mul     r1.xyz,  r0.xyz,  c0.xxx
mad     r2,      r1,      c1,    c2
add     r3.xy,   v0.xy,  -c2.xy
add.sat o0,      r2,      r3
dp4     o0.x,    c0,      r0
dp3     r0.x,    v1.xyz,  v1.xyz
dp2     r0.x,    r0.xy,   r0.xy
tex     r4,      v0.xy,   tex0

; Predicate set + use
setp_lt p, r0.x, c4.x
(p)     kil
(p)     mov r5, c5
(!p)    mov r5, c6

; Structured CF
if_p
    mov r5, c5
else
    mov r5, c6
endif

loop 32
    setp_ge p, r0.x, c1.x
    (p) break
    mad r0, r0, r1, c0
endloop
```

### 8.1 Modifier syntax
- `-src` = negate
- `|src|` = absolute(也支援 `.abs` 後綴)
- `-|src|` = abs then negate
- `op.sat dst` = saturate result

### 8.2 Swizzle / mask shortcuts
- `.xyzw` = identity(default,可省)
- `.x` / `.xx` / `.xxx` / `.xxxx` = replicate
- `.xy` / `.xyz` 等 partial:在 ALU 視為 `.xyXX`(undef)+ 對應 wmask

## 9. Validation

Phase 0 exit gate:
1. 3 reference shader(`tests/shader_corpus/ref_shader_{1,2,3}`)用 v1.0 syntax 改寫完成
2. Assembler / disassembler round-trip = identity
3. ISA simulator output bit-exact 匹配 sw_ref(non-transcendental)
4. Transcendental 在 3-ULP tolerance 內

## 10. References

- [`specs/isa.yaml`](../specs/isa.yaml) — SSOT
- [`tests/shader_corpus/ISA_VALIDATION.md`](../tests/shader_corpus/ISA_VALIDATION.md)
- OpenGL ES Shading Language Spec 1.00
- SPIR-V 1.0 spec
