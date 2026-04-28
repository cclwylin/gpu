# Ch 3 — Simulation strategy

> 6-section outline。讀者讀完 Ch 2 知道 ISA 怎麼長、怎麼解碼;這章
> 把 ISA 之上的 **三層 simulation reference** 一起搬上桌,並用同一顆
> triangle 證明它們互相同意。從這章之後,每一個「X works」的
> 宣告都要在三個 layer 都成立。

## Goal

讓讀者在 60 分鐘內 **跑同一顆 triangle 過三條 backend**(`sw_ref` /
SystemC LT / SystemC CA),拿到 byte-equal 的 PPM,並在心中建立
「為什麼三層都要在」的 mental model。離開這章時,後面所有 sprint 故事
裡「sw_ref 對、SC 不對」這種 phrase 不需要解釋就懂。

## Concepts

讀者要帶走的 5 個 idea:

1. **Three references, one ISA contract** — 從 Ch 2 帶下來的
   `ThreadState` 是合約;sw_ref / LT / CA 三個 backend 全部實作這
   個合約,所以同一個 instruction stream 在三邊跑出同樣的 transition
   sequence。每一層存在的理由不同:
   - `sw_ref` = truth(無 clock,golden 行為)
   - SystemC LT = transaction 順序 + payload(無 clock,但有
     producer / consumer)
   - SystemC CA = cycle-by-cycle timing,跟 RTL 之間的合約
2. **Why three** — 一條 reference 不夠,因為「演算法對不對」跟
   「時序對不對」是兩個不同問題。sw_ref 答前者、CA 答後者、LT 是
   橋。任意兩個對不上,bug 一定在你以為兩個都對的那層中間。
3. **Scene record / replay** — 在 GL API 層攔一次,把
   `clear / draw / state / viewport` 序列化成 plain-text scene
   file。然後 backend 隨便換 — sw_ref 跟 SC 都吃同一份 scene。這
   讓 conformance 跟 parity 變成 **同一條 pipeline 的兩邊量**,
   而不是兩個獨立 system。
4. **Flavour-suffix convention** — file name 跟 class name 的後綴
   表達 SystemC abstraction level:`_lt`(LT、`b_transport`)/
   `_ca`(cycle-accurate、`sc_signal` + `SC_CTHREAD`)/
   reserved `_at`(approximately-timed)/ `_pv`(programmer's view,
   future)。一眼看出在哪個 layer。
5. **Adapter as separate module** — CA chain 的 4 個 adapter
   (SC→PA、PA→RS、RS→PFO、PFO→TBF)是獨立 module,不是折進
   producer 或 consumer。原因是 tb 要在任一邊塞 stub 或 trace,
   adapter 獨立 = swap point 獨立。

## Code walk

### 3.1 — The contract:`ThreadState` 從 Ch 2 帶過來

[`compiler/include/gpu_compiler/sim.h`](../compiler/include/gpu_compiler/sim.h)。
讀者看一眼:

```cpp
struct ThreadState {
    std::array<Vec4, 32> r{};        // GPRs
    std::array<Vec4, 32> c{};        // constants (warp-shared)
    std::array<Vec4,  8> varying{};  // FS inputs
    std::array<Vec4,  8> o{};        // outputs
    bool predicate = false;
    bool lane_active = true;
    int  varying_count = 0;
};
```

這是合約。三個 backend 都實作 `execute(code, state)` 把 instruction
stream 對 `state` 套用一次。

### 3.2 — sw_ref pipeline

[`sw_ref/src/pipeline/`](../sw_ref/src/pipeline/) 一個 dir 八個 file:

```
vertex_fetch.cpp        ← attribute → ThreadState.r
vertex_shader.cpp       ← run VS, ThreadState.o → Vertex
primitive_assembly.cpp  ← clip → cull → viewport → screen-space
rasterizer.cpp          ← edge fn + barycentric + 4× MSAA
fragment_shader.cpp     ← run FS, ThreadState.o → Fragment.color
per_fragment_ops.cpp    ← depth + stencil + blend + colorMask
texture.cpp             ← bilinear sample
resolve.cpp             ← MSAA sample → final pixel
pipeline.cpp            ← 把上面接起來,提供 draw(ctx, count)
```

這就是 truth。沒有 clock、沒有 thread、純算術。書這章只用 `pipeline.cpp`
最外圍的 `draw()` 入口示範,8 個 file 的細節各章拆開講。

### 3.3 — SystemC LT chain

[`systemc/blocks/`](../systemc/blocks/) 15 個 block + adapters。LT
flavour 用 `b_transport`,典型 file 看:

- [`systemc/blocks/perfragmentops/src/perfragmentops_lt.cpp`](../systemc/blocks/perfragmentops/src/perfragmentops_lt.cpp)
  — LT 的 PFO 直接呼叫 `gpu::pipeline::per_fragment_ops`。LT 的
  「演算法」就是 sw_ref 的演算法 — 沒重寫,只是塞進 TLM payload
  抽象層。
- [`systemc/top/src/gpu_top.cpp`](../systemc/top/src/gpu_top.cpp) —
  把 15 個 block 接起來,提供 `gpu_top` SC module。

書這章不細講 TLM-2.0(留給 Ch 12),只指出兩個 invariant:

1. LT 跟 sw_ref **演算法相同**(LT block 內部 call sw_ref 函式)。
2. LT 用 `b_transport` 把 transaction 在 block 之間 push,任何
   sequence bug 都會在 LT 跟 CA 的對比中暴露,而不會被「演算法
   bug」混進來。

### 3.4 — SystemC CA chain

[`systemc/blocks/`](../systemc/blocks/) 同一個 dir,但每個 block
還有 `_ca` flavour(15 個 block 全部都有 CA 版本):

```
commandprocessor_ca.cpp
shadercore_ca.cpp
vertexfetch_ca.cpp
primitiveassembly_ca.cpp
rasterizer_ca.cpp
... (10 more)
```

CA 用 `sc_signal` + `SC_CTHREAD`,每個 hop 之間靠 valid / ready
handshake。Adapter 在中間:[`systemc/blocks/adapters/`](../systemc/blocks/adapters/)
四個 file(sc_to_pa / pa_to_rs / rs_to_pfo / pfo_to_tbf),每個都是
single-buffered pointer-passing。

書這章 demo 一個 CA block:[`systemc/blocks/commandprocessor/src/commandprocessor_ca.cpp`](../systemc/blocks/commandprocessor/src/commandprocessor_ca.cpp)
的 main loop,讓讀者看到 cycle 是怎麼一拍一拍走的。完整 CA 細節
留到 Ch 14。

### 3.5 — Scene record / replay

讀者第一次見到的「跨 backend 同一個 workload」就在這裡:

- **glcompat** 把 GLES 2.0 API call 攔下來,寫成 plain-text scene:
  [`glcompat/src/glcompat_render.cpp`](../glcompat/src/glcompat_render.cpp)
  的 `save_scene_impl`。每個 batch 一段、每個 clear 一行。
- **sw_ref scene_runner**:[`tests/conformance/scene_runner.cpp`](../tests/conformance/scene_runner.cpp)
  讀 scene、餵 `sw_ref`、存 PPM。
- **SC pattern runner**:[`tests/conformance/sc_pattern_runner.cpp`](../tests/conformance/sc_pattern_runner.cpp)
  讀同一份 scene、餵 CA chain、存 PPM。

這三個搭起 Ch 3 hands-on 的核心 demo。

### 3.6 — Three-backend triangle proof

[`tests/glmark2_runner/scene_to_sc.cpp`](../tests/glmark2_runner/scene_to_sc.cpp)
是書裡第一個「三 backend 結果一致」的 ctest entry。它做四件事:

1. 用 `HeadlessCanvas` + glcompat 跑 GLES 2.0 三角形,sw_ref 出
   PPM。
2. 同時 capture scene。
3. fork `sc_pattern_runner` 跑同一個 scene 過 CA chain,出 PPM。
4. 比對 sw_ref PPM ↔ SC PPM,在 apex / bottom-left / bottom-right
   三個取樣點各檢一個 dominant red / green / blue。

這支 test 在 ctest 裡叫 `glmark2.to_sc`。書這章每個 reader 走完
hands-on 之後都應該看到它印 PASS。

## Hands-on

```sh
source .venv/activate
cmake --build build-x86_64 -DGPU_BUILD_SYSTEMC=ON -j

# (1) sw_ref alone — 從一條 hand-built scene 餵
ctest --test-dir build-x86_64 -R conformance.triangle_white -V
# 印 PPM 路徑 + RMSE 0.0

# (2) SC chain alone — 同一個 scene 過 sc_pattern_runner
ctest --test-dir build-x86_64 -R conformance.triangle_white.sc -V
# 印 PPM 路徑 + RMSE < 1.0

# (3) glcompat 帶 ES 2.0 三角形跑 — 三個 backend 一次走完
ctest --test-dir build-x86_64 -R glmark2.to_sc -V
# 印「968 painted px, apex=red, bl=green, br=blue」

# (4) 自己看 PPM:三張並排,肉眼確認 byte-equal
open build-x86_64/glmark2_to_sc.sw.ppm \
     build-x86_64/glmark2_to_sc.sc.ppm
```

讀者跑完看到三件事:`triangle_white` 在 sw_ref 跟 SC 兩條獨立
ctest 都過;`glmark2.to_sc` 在同一個 process 裡用 ES 2.0 path 證明
sw_ref ↔ CA chain 一致;PPM 用 hex viewer 看 byte 完全相等。如果
其中任何一個失敗,後面所有 chapter 的 multi-abstraction parity
故事就立刻搖。

## Decisions log

| 決定 | 走 A 不走 B 的原因 | 後來踩到 |
|---|---|---|
| **`sw_ref` 是 truth、不是 SC chain 的 wrapper** | sw_ref 用 `std::*` 算 FP、用 trivial loop 跑 fragment;SC 模 RTL 的 cycle、有 stalls。把 truth 留在最簡單的 layer 才能在 conformance 出問題時「先確認 spec 對不對」 | Ch 11:`sw_ref.pfo` 一度因為 single-precision FMA 對角線丟 coverage(Sprint 44)。解法是把 `edge_fn` 中間值 promote 到 double — sw_ref 自己變更精準,SC 就跟著對了。 |
| **Scene 是自家 plain-text format,不是 GL trace** | GL trace 太大、難讀、一改 driver 就壞;plain-text scene 跟 git diff 友善,bug repro 直接 `cat` 出來貼 issue | Ch 16:scene format 漏掉 per-CLEAR scissor + colorMask(Sprint 60 補)、漏掉 multi-varying(Sprint 61 補)。每次補都是一個小 git diff,沒重寫整個 capture。 |
| **LT 演算法 = sw_ref 演算法**(LT block 直接 call sw_ref 函式) | 不重寫、不會漂掉;LT 跟 sw_ref 對不上一定是 transaction-層 bug,搜尋空間立刻縮一個 dimension | Ch 13:CP LT 跟 CA 對不上時,讀者直接跳過「演算法」嫌疑,專心看 transaction sequence。 |
| **CA adapter 獨立 module** | tb 要在 SC→PA、PA→RS、RS→PFO、PFO→TBF 任一邊塞 stub。adapter 折進 producer = stub 也得改 producer,違反 single-responsibility | Ch 14:Sprint 61 的 `vp_x / vp_y` 漏在 `ScToPaAdapterCa` 裡 — 因為 adapter 獨立,fix 是一個 file 改 4 行,不必動 PA。 |
| **Flavour suffix `_lt` / `_ca` 寫進 file name** | 讀者一看 path 就知道在哪個 abstraction layer。「`commandprocessor_ca.cpp`」比「`commandprocessor.cpp` 裡的某個 namespace」更難搞混 | Sprint 87(refactor)出現過想把 LT 跟 CA 合進同 file 的提案;suffix convention 把這個討論直接擋在 review 階段。 |

## Exercises

1. **三 backend triangle**(easy)。從乾淨 checkout 跑出
   `glmark2.to_sc`,把 sw_ref 跟 SC 的 PPM hash 印出來確認相等。
2. **手寫 scene file**(easy–medium)。讀
   `tests/scenes/multi_batch_state.scene`,改 clear color、改一個
   vertex 的 z,讀 `scene_runner` 跟 `sc_pattern_runner` 的輸出
   差異。
3. **故意打破一邊**(medium)。在 sw_ref 的 `edge_fn` 把 `double`
   intermediate 改回 `float`,rebuild,跑 `conformance.triangle_msaa`
   觀察哪幾個 sample 開始 disagree。再改回去。這個練習讓讀者親手
   感受 "truth 也會有 bug"。
4. **新增一條 backend**(open)。把 sw_ref pipeline 改成不寫 PPM、
   改寫成 OpenGL stream(reference 跑在你的 desktop GPU),把同一個
   scene 餵下去,跟我們的 sw_ref / SC 比 RMSE。讀者會發現「對 ground
   truth」是一個 ill-defined 問題 — 不同 IHV 在 boundary case 互相
   不同意。

## 章節 anchor

| anchor | 內容 |
|---|---|
| `compiler/include/gpu_compiler/sim.h` | `ThreadState` 合約 |
| `sw_ref/src/pipeline/pipeline.cpp` | sw_ref `draw()` entry |
| `sw_ref/src/pipeline/*.cpp` | sw_ref 8 個 stage |
| `systemc/blocks/perfragmentops/src/perfragmentops_lt.cpp` | LT block 範例(call sw_ref) |
| `systemc/blocks/commandprocessor/src/commandprocessor_ca.cpp` | CA block 範例 |
| `systemc/blocks/adapters/` | 4 個 single-buffer pointer-passing adapter |
| `glcompat/src/glcompat_render.cpp` | scene capture |
| `tests/conformance/scene_runner.cpp` | scene → sw_ref → PPM |
| `tests/conformance/sc_pattern_runner.cpp` | scene → CA chain → PPM |
| `tests/glmark2_runner/scene_to_sc.cpp` | 三 backend triangle proof |
| ctest `conformance.triangle_*` / `glmark2.to_sc` | hands-on green baseline |

## chapter-end tag

`git tag ch03-end` 落在 Sprint 39 之後第一個 `glmark2.to_sc` 通過的
commit(SC chain 跑出 968 painted px 跟 sw_ref bit-identical)。
讀者 checkout 這個 tag、跑 `ctest -R "conformance.triangle|glmark2.to_sc"`
應該全綠。
