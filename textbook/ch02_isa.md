# Ch 2 — The ISA

> 6-section outline。每一節先寫到 sub-bullet 級,正文章節在後面對照
> repo line range 補進去。預設讀者讀過 plan.md 的 Ch 1,知道這顆 GPU
> 的 scope 是 GLES 2.0 + 4× MSAA。

## Goal

讓讀者在 90 分鐘內能 **寫一行 assembly、看到它變成 64-bit instruction、
餵進 simulator、看 ThreadState 怎麼變**。離開這章時,後面所有 chapter
的 ISA-level discussion 都在腦子裡有具體的 bit-pattern 對應。

## Concepts

讀者要帶走的 5 個 idea:

1. **Fixed-width 64-bit instruction** — 三種 format(ALU / FLOW / MEM)
   共用 high-6 bit 做 opcode tag。沒有 ld/st 跟 ALU mix instruction
   的 superscalar 路;一個 instruction 走一條 pipeline path。
2. **3-source ALU + write mask** — `dst = op(s0, s1, s2)` 寫進
   4-channel `dst.wmask`。`fma`、`cmp`(三項選擇)、`mad` 等都能
   一條 instruction 表達。
3. **Source class:GPR / CONST / VARYING / IMM** — 2 bit 一個 src
   class,讓 register file 跟 constant bank 跟 interpolated
   varying 共享 instruction encoding 空間。s2 沒有 class field —
   這個 trade-off 後面會回來咬一口(Ch 11)。
4. **Predicate-driven divergence** — `setp` 寫 per-lane predicate,
   `if_p` / `else` / `endif` 讓 16-thread warp 在不同分支下執行
   masked。沒有 explicit branch instruction 在 lane 層。
5. **ThreadState 是合約** — `r[32]` GPR、`c[32]` constant bank、
   `varying[8]` per-fragment input、`o[8]` output。所有 backend
   (sw_ref / SystemC LT / SystemC CA)都實作這個合約;同樣的
   instruction stream 跑同樣的 ThreadState transition。

## Code walk

### 2.1 — Bit layout

從 [`compiler/include/gpu_compiler/encoding.h:35-145`](../compiler/include/gpu_compiler/encoding.h#L35-L145) 進場。三段:

- `AluFields` + `encode_alu` + `decode_alu`(行 35–109)
- `FlowFields` + `encode_flow` + `decode_flow`(行 111–135)
- `MemFields` + `encode_mem` + `decode_mem`(行 137–214)

每一段先看 layout comment、再看 struct、再看 encode 函式 — 這順序
讓 reader 把每個 field 的 bit position 跟它的語義對齊。

特別點出兩個地方:

- **`dst_class` 1 bit**:0 = GPR(`dst[4:0]` 用全 5 bit),1 = output
  (`dst[2:0] = o0..o7`,`dst[4:3]` reserved)。Sprint 58 的
  `& 0x07` 從 2 bit widen 到 3 bit 是這本書最早出現的 ISA bump。
- **s2 沒有 class field**:省了 2 bit 給 immediate 用,但 cmp 的
  third operand 永遠是 GPR。這個 decision 在 Ch 11 變成 GPR
  pressure 故事的開端。

### 2.2 — Opcode list

`compiler/assembler/gen/opcodes.h` 是 spec-driven 自動生出來的;
spec 在 [`docs/isa_spec.md:189`](../docs/isa_spec.md#L189) `## 5.
Opcode List`。書裡只列「每個 reader 至少要看過一遍」的 8 個 opcode:

| op | mnemonic | format | 用在 | line |
|---:|---|---|---|---|
| 0x01 | mov | ALU | shadow update、broadcast | 第一次出場 Ch 7 |
| 0x02 | add | ALU | 4-vec add、b-a 用 s1_neg | Ch 5 barycentric |
| 0x03 | mul | ALU | per-channel multiply | Ch 9 mat × vec |
| 0x06 | dp4 | ALU | mat row × vec、vec equality reduce | Ch 11 |
| 0x13 | cmp | ALU | `(s0 ≥ 0) ? s1 : s2` | Ch 11 boolean lowering |
| 0x18..1D | setp_* | ALU | predicate(EQ/NE/LT/LE/GT/GE) | Ch 11 if-cond |
| 0x26..28 | if_p / else / endif | FLOW | divergence | Ch 6 |
| 0x34 | tex | MEM | bilinear texture fetch | Ch 5 後半 + Ch 15 |

完整 list 進 appendix。

### 2.3 — Assembler

[`compiler/assembler/src/asm.cpp`](../compiler/assembler/src/asm.cpp)
460 行,結構簡單:tokenize → parse line → fill `AluFields` /
`FlowFields` / `MemFields` → `encode_*`。讀者只要看三個段:

- swizzle 字串 `.xyzw` / `.rgba` / `.stpq` → 8-bit byte(行 80–110)
- operand parse(GPR `r5` / CONST `c12` / VARYING `v0` / IMM `#1.5`)
- 整條 line 的 dispatch table(行 280 起)

### 2.4 — Disassembler

[`compiler/assembler/src/disasm.cpp`](../compiler/assembler/src/disasm.cpp)
177 行,是 assembler 的 inverse。書裡用它示範「encode 之後 decode 回
原字串」的 round-trip — 這也是 ctest entry `compiler.asm_roundtrip`
做的事。

### 2.5 — Single-thread simulator

[`compiler/isa_sim/src/sim.cpp`](../compiler/isa_sim/src/sim.cpp) 295
行。三個關鍵函式:

- `read_src` / `fetch_src`:從 `ThreadState` 讀 GPR / CONST /
  VARYING / IMM,套 swizzle / neg / abs。
- `dst_lvalue`:回傳 `Vec4&` 給 `write_masked` 寫。Sprint 58 的
  `dst & 0x7` 在這裡。
- 主迴圈:`fetch_src` × 3 → `dispatch.inc` → `write_masked`。
  Dispatch 是 spec 自動生的 [compiler/isa_sim/gen/dispatch.inc](../compiler/isa_sim/gen/dispatch.inc),保證 spec 跟 sim 不會
  漂掉。

### 2.6 — 16-thread warp simulator

[`compiler/isa_sim/src/sim_warp.cpp`](../compiler/isa_sim/src/sim_warp.cpp)
298 行。跟單 thread 版的 diff:per-lane mask、`if_p` 開 divergence
stack、`break` 把 lane 從 active set 拿掉。書裡只解釋 mask 機制,
完整 SIMT 細節留到 Ch 6。

## Hands-on

```sh
source .venv/activate
cmake --build build-x86_64 --target gpu-asm gpu-disasm gpu-isa-sim

# (1) 寫 6 行 assembly,跑 round-trip
cat > /tmp/hello.asm <<'EOF'
mov r0, c0          ; pos = c0
add r1, r0, c1      ; r1 = pos + bias
dp4 r2.x, r1, r1    ; |r1|^2
mul r3, r1, r2.xxxx ; r1 * |r1|^2
mov o0, r3          ; output
EOF
build-x86_64/compiler/gpu-asm /tmp/hello.asm /tmp/hello.bin
build-x86_64/compiler/gpu-disasm /tmp/hello.bin
# 應該逐字印回原 asm。

# (2) ctest 跑 round-trip:60 random instruction encode→decode→encode 一致
ctest --test-dir build-x86_64 -R compiler.asm_roundtrip

# (3) sim 跑一條 mov、看 ThreadState
build-x86_64/compiler/gpu-isa-sim --asm /tmp/hello.asm --dump-state
```

讀者跑完看到三個東西:disassembler 印出的字串跟原 asm 一致(round-
trip 成立);ctest entry 印 PASS;sim 的 state dump 顯示 r0..r3
按照預期變化。如果這三件事在乾淨 checkout 上不成立,就是這章寫錯
而不是讀者環境壞。

## Decisions log

| 決定 | 走 A 不走 B 的原因 | 後來踩到 |
|---|---|---|
| **5-bit GPR field**(32 個 register) | 64-bit instruction 已經很擠;32 個 GPR 對 GLES 2.0 fragment shader 夠用 | Ch 11:wide random shader 滿載 GPR 後 `& 0x1F` silent-wrap,踩到 attribute slot。Sprint 57 加 `cached_zero_gpr / cached_one_gpr` 把 cmp 從 4 GPR 壓到 1 GPR。 |
| **s2 沒有 class field** | 省 2 bit 給 immediate;絕大多數 ALU 用法 s2 是 GPR | Ch 11:cmp 的 false lane 永遠要 GPR materialize。在 GPR pressure 緊的 shader 裡得用 cached `zero_gpr` 撐住。 |
| **4 個 output slot**(`dst_class=1` 只 2 bit) | 第一版 spec 估「gl_Position + 3 個 varying 應該夠」 | Sprint 58 widen 到 8(3 bit)— `basic_shader.{23,24,29,36,96}` 用 4+ vec4 varying 直接踩到。Ch 11 會走完這次 ISA bump 跨 layer 的 cost。 |
| **Predicate-driven divergence**(沒有 lane-explicit branch) | SIMT 標準做法;`if_p` / `else` / `endif` 三條 instruction 就把 divergence stack 表達完 | Sprint 57:`setp` 是 scalar、只看 `s0[0]` / `s1[0]`,vec `==` 必須在 codegen 層 dot-reduce 成 scalar 再 setp。讀者要記得 setp 不會自己擴散。 |

## Exercises

1. **重現 round-trip**(easy)。寫 12 條手刻 instruction,組進去、
   拆出來、再組一次,證明兩次 binary 完全一致。
2. **量 encoding 餘裕**(medium)。給 `AluFields` 全部 reserved bit
   都當成 0,計算還剩多少 free bit;選一個 plausible extension(例如
   per-instruction predicate hint)看怎麼塞進去。
3. **修一個 silent-wrap**(medium)。把 `dst & 0x1F` 改成檢查
   overflow 後 `error()`;rebuild,觀察哪些 random-shader compile
   會直接斷在 codegen,而不是 silent 出錯。
4. **設計題:6-bit GPR**(open)。如果 GPR field 拉到 6 bit(64
   register),要從 64-bit instruction 哪兩個 bit 偷出來?設計三個
   候選,每個列出後面 Ch 11 的故事會怎麼變。

## 章節 anchor

| anchor | 內容 |
|---|---|
| `compiler/include/gpu_compiler/encoding.h` | 三種 format 的 layout |
| `compiler/include/gpu_compiler/sim.h` | `ThreadState` 跟 `WarpState` |
| `compiler/assembler/src/asm.cpp` | text → 64-bit |
| `compiler/assembler/src/disasm.cpp` | 64-bit → text |
| `compiler/isa_sim/src/sim.cpp` | single-thread sim |
| `compiler/isa_sim/src/sim_warp.cpp` | 16-thread warp sim |
| `compiler/isa_sim/gen/dispatch.inc` | spec-generated opcode dispatch |
| `compiler/assembler/gen/opcodes.h` | spec-generated opcode enum |
| `docs/isa_spec.md` | spec single source of truth |
| ctest `compiler.asm_roundtrip` / `compiler.sim_basic` / `compiler.sim_warp` | hands-on green baseline |

## chapter-end tag

`git tag ch02-end` 落在這個 commit:**`5240f5e`**(Phase 0 ISA freeze)
+ Sprint 58 的 ISA widen(`encoding.h` 拉到 8 outputs)。讀者
checkout 這個 tag 跑 `ctest -R compiler.{asm_roundtrip,sim_basic,sim_warp}`
應該全綠。
