# Ch 11 — Compiler under stress: VK-GL-CTS `basic_shader`

> 6-section outline。預設讀者讀過 Ch 8(GLSL frontend)+ Ch 9(codegen)
> + Ch 2(ISA)+ Ch 17 開頭(`glcompat` shim 怎麼把 dEQP 接到我們)。
> 這章是全書的「壓力測試」章 — 前面 8 章我們的 compiler 只見過 25 顆
> 自家寫的 corpus shader,這章它要面對 dEQP random-shader generator
> 一次餵進來的 100+ 顆形狀千奇百怪的 shader,然後不能爆。

## Goal

讓讀者在 90 分鐘內走完 **四個從 image-cmp diff 倒推到 root cause 的
bug story**,順序就是當時診斷的順序。離開這章時:

- 看到 dEQP 的 image-cmp `(0, 64, 128, 0)` vs ours `(0, 0, 128, 0)`
  之類的 diff,腦中能直接列出三個可疑層(uniform 沒上、literal 衝
  slot、`bool(x)` 被當 identity)。
- 知道為什麼 c-bank 寬度是 cross-cutting decision — 從 ISA 起、
  影響 sw_ref 跟 SC chain 兩個 backend、最後反過來決定 compiler
  能 admit 多少 random shader。
- 讀者 checkout `ch11-end` 跑 `tools/run_vkglcts_sweep.py`,看到
  `fragment_ops.* 1887/1923 (98.1 %)` — 跟書裡寫的數字逐字相同。

## Concepts

讀者要帶走的 5 個 idea:

1. **Random-shader 是 compiler 的灰盒測試** — dEQP 的
   `interaction.basic_shader.*` 100 case 不是手寫的「測這個 feature」,
   而是 PRNG 在 GLSL ES 1.0 grammar 上隨機展開。它測的是 frontend
   有沒有 admit 整個 grammar、admit 之後 codegen 會不會在 GPR / c-bank
   壓力下出錯。前者用 compile error rate 量,後者用 image-cmp RMSE 量。

2. **同一個 image-cmp diff 可能對應到差很多層的 bug** — 一個 G
   channel 整 row 是 0 而 ref 是 255,可以是:(a) shader 寫
   `gl_FragColor.g = c` 但 `c` 被別人 clobber、(b) `bool(x)` ID-pass
   把 0.25 當 G 寫進去、(c) blend factor 拿到錯的 alpha。逆推三個層
   要靠的是 **histogram 對齊 + per-channel max diff** 的二步法。

3. **C-bank 是 cross-stage shared resource** — VS 跟 FS 編譯時各自
   不知對方,但 runtime 上同一個 `Vec4f[N]` 被兩邊讀。Naive uniform
   slot 0 / literal slot 15 兩家都拿,bake order 一決定誰先寫,行為
   就 non-deterministic。修法是把「下一個可用 slot」當 compile API
   的 input,而不是兩邊各自從 0 / 16 開始長。

4. **ISA 寬度跟 random-shader admit rate 是同一條 trade-off curve** —
   c-bank 16 vs 32 不是「越大越好」的單向 vote;encoding bit、sim
   register 數、SC chain payload 都會跟著動。這章會走完一次
   16 → 32 的 cross-layer change,讓讀者看到「動 compiler 一個常數,
   要碰到 5 個 file」。

5. **Boundary case 才是壓力測試的真正內容** — `LESS` / `EQUAL` /
   `GEQUAL` / `NOTEQUAL` 四個 depth func 在「frag.z 跟 buffer.z 字面
   相等」時行為不同,而 dEQP 視覺化 quad 的 z 又剛好跟 base render
   的 z 對齊。`l0 + l1 + l2 != 1.0` 的 1 ULP 漂移會把 strict 邊界
   推到錯邊。這個 bug 嚴格說是 rasterizer 的、不是 compiler 的,但
   診斷它要從 compiler 看出來 — 收尾這章。

## Code walk

四個 bug 都用同一個格式:**(a) image diff → (b) 假設 → (c) probe
最小 repro → (d) fix in one line + 多動到的 file → (e) 後續 sweep
數字**。

### 11.1 — `bool(x)` identity-pass(`basic_shader.0` 的 G=0)

**Image diff。** [`fragment_ops.interaction.basic_shader.0`](../tests/VK-GL-CTS/modules/gles2/functional/es2fRandomFragmentOpTests.cpp)
result `(0, 0, 128, 0)` × 1504,reference `(0, 255, 128, 0)` × 1504。
G channel 落差 255。Histogram 告訴我們「ref 跟 ours 兩個 region 大小
完全一樣 (2272 / 1504 / 320)」 — 這是兩邊都畫到、但畫錯顏色的
strong signal。

**假設。** Shader 是 `gl_FragColor = vec4(d, c, b, a)`,其中 `b` 是
`const bool b = -2 < int(0)` (= true = 1.0) 拿到 G channel。如果我們
的 codegen 把 `b` 當成 `bool(NUMBER)` 直接 identity-pass,那 G 就會
拿到「`-2 < int(0)` 那個 expression 直接的 evaluation」而不是
「(評估後 != 0 ? 1.0 : 0.0)」。

**Probe。** 寫 [`/tmp/probe_bool.cpp`](../tools/regress_examples.py)(範例)
直接跑 `gpu::glsl::compile()`,FS 為 `bool(0.25)` 寫進 vec4 看
G byte。預期 255,實得 64 — 0.25 × 255 ≈ 63.75 → 取整 64。**confirm**。

**Fix。** [`compiler/glsl/src/glsl.cpp:1054-1098`](../compiler/glsl/src/glsl.cpp#L1054-L1098)。
Constant-fold `bool(NUMBER)` 在 compile 時:`v != 0 ? 1.0 : 0.0`。
非 literal 的 runtime path 走 `cmp(x*x − ε, 1, 0)` — 3 ALUs。

只動 1 個 file。

**Sweep。** `basic_shader.* 12/100 → 38/100 (+26)`。

### 11.2 — VS/FS c-bank slot 衝突(`basic_shader.0` 過了，`basic_shader.4` 全黑)

**Image diff。** 39 個 basic_shader 過,61 個 fail。其中
`basic_shader.4` result 整張 = clear color `(0, 64, 128, 255)`,
reference 有實際的 colored region。Histogram「全 clear color
4096 / 4096」 — 這次不是「畫錯」,是「啥都沒畫」。

**假設。** VS 跑了沒輸出 valid `gl_Position`?還是 FS 拿不到 uniform?
看 [`compiler/include/gpu_compiler/glsl.h:25-50`](../compiler/include/gpu_compiler/glsl.h#L25-L50)
的 `CompileResult.uniforms` ordering — VS uniform `f` slot=0,FS
uniform `c` slot=0。**Two stages, same slot**。Run-time
[`glcompat/src/glcompat_es2.cpp:471-490`](../glcompat/src/glcompat_es2.cpp#L471-L490)
裡 `for (auto& [name, loc] : baked->uniform_loc)` 在 `unordered_map`
上跑 — 順序 implementation-defined。FS 的 `c=1` 跟 VS 的 `f=-8`
寫到同一個 `ctx.draw.uniforms[0]`,後寫的贏。

**Probe。** 用 `GLCOMPAT_DUMP_SHADER_SOURCE=1` dump basic_shader.4 的
shader,手刻 minimal probe,對 `c=1` `f=-8`,跑 5 次看結果。三次
0.5、兩次 1.0 — non-deterministic confirm。

**Fix。** [`compiler/include/gpu_compiler/glsl.h:54-60`](../compiler/include/gpu_compiler/glsl.h#L54-L60)
+ [`compiler/glsl/src/glsl.cpp:1419-1430`](../compiler/glsl/src/glsl.cpp#L1419-L1430)
+ [`glcompat/src/glcompat_es2.cpp:218-243`](../glcompat/src/glcompat_es2.cpp#L218-L243)。

`compile()` 拿 3 個新參數:

```cpp
CompileResult compile(const std::string& source, ShaderStage stage,
                      int uniform_slot_base = 0,        // VS = 0; FS = vs_uniform_top
                      int literal_slot_top  = 32,       // VS = 32; FS = vs_literal_bottom
                      const std::vector<LiteralBinding>& preset_literals = {});  // VS pool to share with FS
```

Glcompat 編 VS、計算 `vs_uniform_top` + `vs_literal_bottom`、塞進 FS。
Uniform 從 0 往上、literal 從 32 往下,不會撞到。Literal 池跨 stage
dedup:相同 value 共用 slot。

只動 3 個 file。

**Sweep。** `basic_shader.* 38/100 → 64/100 (+26)`。

### 11.3 — C-bank 從 16 widen 到 32(ISA 沒動,storage 動了)

**為什麼是 11.2 的 follow-up?** 11.2 把 VS/FS 拆開以後,某些 shader
的 uniform + literal 加起來 **超過 16**。隨機 shader 可以一輪用到
1+5 = 6 個 uniform、外加 10-13 個 distinct literal。VS+FS 兩家加起來
20+ 個 c-bank slot。`Vec4f[16]` 會 silent-overflow — 我們存進
slot 17 的值最後跑哪去看 `std::array.operator[]` 的 UB 心情。

**Probe。** 把 [`compiler/glsl/src/glsl.cpp:618`](../compiler/glsl/src/glsl.cpp#L618)
的 `intern_constant` 加一行 assert(`slot < 16`),rebuild,跑 sweep
— 11 個 shader 直接抓出 assert,印 over-16 slot allocation。**confirm**。

**Fix — 但這次動到 5 個 file**。ISA 那邊 [`encoding.h`](../compiler/include/gpu_compiler/encoding.h#L50)
的 `s0idx` 已經 5 bit(0..31),所以 ISA 不用動。但是 **storage** 三個地方在跟著:

- [`sw_ref/include/gpu/state.h:113`](../sw_ref/include/gpu/state.h#L113) — `DrawState::uniforms` 從 `Vec4f[16]` → `Vec4f[32]`
- [`compiler/include/gpu_compiler/sim.h:31`](../compiler/include/gpu_compiler/sim.h#L31) — `ThreadState::c` 16 → 32(WarpState 自動跟)
- [`sw_ref/src/pipeline/{vertex,fragment}_shader.cpp`](../sw_ref/src/pipeline/fragment_shader.cpp#L34) — copy 迴圈 `for (i = 0; i < 16; ++i)` → `< 32`
- [`compiler/glsl/src/glsl.cpp:611`](../compiler/glsl/src/glsl.cpp#L611) — `const_pool_base` 預設 16 → 32

5 個 file,加總 ~10 行 diff。但跨了 ISA-spec / sw_ref / sim / compiler
四個 layer。讀者第一次看到「動 compiler 一個常數,牽一髮動全身」。

**Sweep。** `basic_shader.* 64/100`(沒升,因為這只是讓 11.2 不會
overflow)。但 sweep 整體沒掉 — 重要的 negative result。

### 11.4 — 收尾:constant-depth quad bypass(`stencil_depth_funcs` 49/81 → 81/81)

**為什麼放在 compiler 章?** 它是 rasterizer 的 1 行 fix,但診斷它
**完全靠 compiler 角度看**:dEQP 的 visualization quad 是
constant-depth(4 個 vertex 的 z 全部一樣),所以 `frag.depth` 應該
等於 quad.z **而不是 barycentric interpolation 的近似**。

**Image diff。** `stencil_depth_funcs` 的 fail pattern 不是隨機 —
而是 **8 個 depth func 中剛好那 4 個會在 boundary 出問題的 fail**:
`LESS` / `EQUAL` / `GEQUAL` / `NOTEQUAL`(都 distinguish equality)。
另外 4 個(`LEQUAL` / `GREATER` / `ALWAYS` / `NEVER`)pass。這 pattern
強到不能視為 random — 一定是 boundary。

**Probe。** [Probe with constant-z quad in `tests/conformance/test_constant_depth.cpp`]
(如果讀者跟著重做),手刻 quad 4 個 vertex 都 `z=0.5`,buffer cleared
to 0.5。LESS 應該 false(strict),LEQUAL 應該 true(non-strict)。
我們的 `frag.depth = l0*z0 + l1*z1 + l2*z2` 在 `l0+l1+l2 ≠ 1.0`
的 ULP 漂移下 boundary 飄到錯邊。

**Fix。** 1 行,[`sw_ref/src/pipeline/rasterizer.cpp:166-172`](../sw_ref/src/pipeline/rasterizer.cpp#L166-L172):

```cpp
if (v0.pos[2] == v1.pos[2] && v1.pos[2] == v2.pos[2]) {
    frag.depth = v0.pos[2];
} else {
    frag.depth = l0 * v0.pos[2] + l1 * v1.pos[2] + l2 * v2.pos[2];
}
```

**Sweep。** `stencil_depth_funcs.* 49/81 → 81/81 (+32)`,整個 subgroup
關掉。沒動 compiler 任何一行。

## Hands-on

```sh
source .venv/activate
cmake --build build-arm64 -j 4   # 或 build-x86_64

# (1) 完整 sweep — fragment_ops 應該 1887/1923 (98.1 %)
python3 tools/run_vkglcts_sweep.py --groups dEQP-GLES2.functional.fragment_ops

# (2) 重現 11.1 的 image-cmp diff(checkout ch11-bool-fix-pre 之前)
git checkout ch11-bool-fix-pre
cmake --build build-arm64
build_vkglcts-arm64/modules/gles2/deqp-gles2 \
    --deqp-case='dEQP-GLES2.functional.fragment_ops.interaction.basic_shader.0'
# 看 TestResults.qpa 裡的 Result/Reference image,confirm G=0 vs 255

# (3) 重現 11.2 的 non-deterministic uniform clobber
git checkout ch11-uniform-fix-pre
for i in 1 2 3 4 5; do
    build_vkglcts-arm64/modules/gles2/deqp-gles2 \
        --deqp-case='dEQP-GLES2.functional.fragment_ops.interaction.basic_shader.4' \
        | tail -3
done
# 五次裡有些 Pass 有些 Fail — race confirm

# (4) 11.3 的 over-16 assert(讀者自己加)
# 編 compiler/glsl/src/glsl.cpp 的 intern_constant,在 const_pool_base--
# 之前加 assert(const_pool_base > 16);rebuild,跑 sweep
# 應該抓到 ~11 個 shader trip 這個 assert

# (5) 11.4 的 4-of-8 depth-func pattern
git checkout ch11-end
build_vkglcts-arm64/modules/gles2/deqp-gles2 \
    --deqp-case='dEQP-GLES2.functional.fragment_ops.depth_stencil.stencil_depth_funcs.*'
# 應該 81/81 — 比 ch11-depth-fix-pre 那邊的 49/81 多 32 個
```

`ctest --test-dir build-arm64 -R compiler` 應該全綠 — Sprint 47-56 沒
regress 任何既有 ctest entry。

## Decisions log

| 決定 | 走 A 不走 B 的原因 | 後來踩到 |
|---|---|---|
| **`bool(x)` constant-fold + 3-ALU runtime fallback** | dEQP random shader 99 % 以上的 `bool()` 是 literal 引數,fold 一律 free。Runtime 那 1 % 用 `cmp(x*x − ε, 1, 0)` 不需要新 opcode | 後續 Ch 14 的 SC chain 重做 cmp 時要記得 ε 是 host 那邊 inject 的,不是 ISA 自帶 — `ε = 1e-6` 寫進 literal pool 才 bit-perfect |
| **VS uniform 從 0 長,FS 從 `vs_top` 長** | C-bank 是單一陣列,順著一邊塞、不用 ISA 上加 stage tag | 不能在 link time 之後加 uniform — 加一個就要全部 re-bake。讀者覺得 dynamic uniform layout 比較好的話請看 Ch 13 的 trade-off |
| **Literal 池跨 stage dedup** | dEQP shader 常用相同的 0.0 / 1.0 / 0.5 — dedup 後 32-slot 才夠用 | 把「VS literal `1.0`」跟「FS literal `1.0`」綁在 same slot 表示 future-side decoupled VS/FS compilation 不能做 — patch shader 的工具(沒做)需要繞開這邊 |
| **C-bank 16 → 32(不動 ISA)** | `s0idx` 5 bit 早就支援到 31。動 storage 比動 ISA 便宜 N 倍 — encoder / disassembler / spec / tests 一個都不用碰 | 32 之後就頂到 ISA ceiling 了。下個壓力來了(例如 mat4 uniform 大量出現)只能 spill 或加 stage tag,沒地方往上長 |
| **Constant-depth bypass 做在 rasterizer,不在 compiler** | Compiler 那邊不知 quad 4 vertex 是不是 constant z(per-batch 才知)。Rasterizer 看到 `v0.z == v1.z == v2.z` 是天然的 spot | 三角形剛好斜對角相同的 shader 也會 bypass。ULP 邊界 — 這是接受的近似,書裡記成 known approximation |

## Exercises

1. **重現 4 個 bug 的 diff**(easy)。把 4 個 fix 一個個 revert(用
   `git revert -n <commit>`),跑 `--deqp-case='basic_shader.*'`,
   觀察 fail count 變化。預期看到 64 → 38 → 12 → 0(一個比一個爛)。

2. **量 c-bank 飽和度**(medium)。寫 script 統計 `result.literals`
   + `result.uniforms` 的 slot 用量分布。畫 histogram。對 `basic_shader.*`
   的 100 個 program,看 90 percentile / max。如果是 28,那離 32 還
   有 4 個 slot 緩衝;如果是 31,下次 dEQP update 一定踩到。

3. **修 vec ==**(medium)。我們現在的 `==` 只比 `s0[0]` / `s1[0]`,
   vec `==` 應該用 `dp4(diff, diff)` reduce 成 scalar,再 `setp`。
   寫一個 probe 證明 `ivec4(1, 0, 0, 0) == ivec4(1, 1, 0, 0)` 在我們
   impl 是 true(只看 .x),正確答案是 false(整 vec 不一致)。提交
   patch。

4. **設計題:per-stage c-bank**(open)。如果 ISA 加一個 1-bit
   `c_stage_tag`,VS 跟 FS 各自有 32 slot,11.2 的整套 dedup 邏輯就
   不需要。算這多出來的 1 bit 要從哪兩個 reserve 偷;列出 sw_ref
   / SC LT / SC CA / glcompat 各動哪些行;對「load uniform 帶
   stage info」的 SC payload widen 影響。

## chapter-end tag

`git tag ch11-end` 落在這個 commit:**`40d3e78`** + 補上
`ch11-bool-fix-pre` / `ch11-uniform-fix-pre` / `ch11-depth-fix-pre`
這三個診斷重現用的 mid-sprint tag。讀者 checkout `ch11-end` 跑完
hands-on (1)–(5),`fragment_ops.* 1887/1923`、`stencil_depth_funcs
81/81`,跟書裡逐字相同。

## 章節 anchor

| anchor | 內容 |
|---|---|
| [`compiler/glsl/src/glsl.cpp`](../compiler/glsl/src/glsl.cpp) | parser + codegen + 4 個 fix 都在裡面 |
| [`compiler/include/gpu_compiler/glsl.h`](../compiler/include/gpu_compiler/glsl.h) | `compile()` API + `LiteralBinding` |
| [`glcompat/src/glcompat_es2.cpp`](../glcompat/src/glcompat_es2.cpp) | `try_glsl_compile` — VS top → FS base 計算在這裡 |
| [`sw_ref/include/gpu/state.h`](../sw_ref/include/gpu/state.h) | `DrawState::uniforms` 16 → 32 |
| [`compiler/include/gpu_compiler/sim.h`](../compiler/include/gpu_compiler/sim.h) | `ThreadState::c` 16 → 32 |
| [`sw_ref/src/pipeline/rasterizer.cpp`](../sw_ref/src/pipeline/rasterizer.cpp) | constant-depth bypass 那一段 |
| [`docs/regress_report.md`](../docs/regress_report.md) | Sprint 47-56 詳細 sweep diff |
| ctest `compiler.glsl_compile` / `compiler.glsl_ext` | hands-on 綠 baseline |
| `tools/run_vkglcts_sweep.py --groups fragment_ops` | 真正驗 1887/1923 的東西 |
