# Textbook plan — *Building a GPU from ISA to Conformance*

以 project-based 方式從零走到 conformance 的 GLES 2.0 GPU 實作教學,
**用本 repo 約 60 個 commit 當 running case study**。每一章都會列出
那一章的工作對應到哪些 file、哪些 ctest entry、以及 VK-GL-CTS / SC
parity 的具體數字 — 讀者 checkout 對應 tag、重跑 sweep,看到的
數字應該完全一樣。

## Pitch

大多數 GPU / SoC 教科書把每塊獨立講:「這是 ISA」、「這是 rasterization
數學」、「這是 SystemC TLM」。學生唸完知道詞彙但不知道怎麼接起來 —
不知道第 2 章某個 ISA decision 會在三個月後變成第 11 章的 1060-case
conformance regression,也不知道修它要動到哪幾層。

這本書的設計是 **整本書圍繞一個專案**,專案隨章節長大。讀完之後
讀者已經(或讀完了)做出:

- 一個 GLES 2.0-class ISA + assembler + simulator
- 一個 software reference renderer(`sw_ref`)
- 一個 GLSL → SPIR-V → ISA compiler frontend
- 一條 cycle-accurate SystemC chain(15 個 block + 4 個 adapter)
- 一個跑 VK-GL-CTS、同時驗到兩個 layer 的 conformance harness
- 以及把它們之間 gap 補起來的 diagnostic 工具鏈

教學上的核心 hook 是 **bit-for-bit traceability**。第 11 章說
「fragment_ops 1920 / 1923 case 通過 sw_ref」時,讀者可以 checkout
chapter-11 tag、跑 `tools/run_vkglcts_sweep.py`,看到一模一樣的
數字。第 14 章說「19 / 19 color_clear 透過 SC chain 達 bit-perfect」
時,讀者跑 `tools/run_vkglcts_to_sc.py --group color_clear`,看
SC PPM 的 byte 跟 sw_ref 的 render 完全相等。

## Audience

主讀者:大四(architecture / VLSI / graphics 選修)或研一,修過基礎
computer architecture + 任一門(compiler / graphics / SystemC)。

次讀者:在業界把每塊各自學過(ISA、rasterizer、SystemC),但缺一個
端到端範例把它們黏起來的工程師。

預設讀者已經有的能力:

- C++17 流暢。C++20 加分但非必要。
- Linear algebra 到「不查表能算 4×4 × vec4」的程度。
- 接觸過 OpenGL pipeline,「曾經畫過一顆三角形」即可。不需要 GLSL
  進階經驗。
- 看 Makefile / CMakeLists 不會卡。書會點到 file 但不會花章節教
  build system。

書 **不假設** 讀者已經會:

- SystemC(Ch 12 從零開始介紹)
- VK-GL-CTS / dEQP(Ch 17)
- cache coherence、AXI、RTL(Phase 3 只用 sketch 章節帶過)

## Structure

六個 part、約 22 章。每章固定六個段落:

1. **Goal** — 一句話目標。
2. **Concepts** — 這章需要的 idea(3–6 個)。
3. **Code walk** — 指到 repo 的 file + line range,每段附短解釋。
4. **Hands-on** — 一個 checked-in 的 script 或 ctest;讀者跑不出來
   就是這一章寫壞了。
5. **Decisions log** — 1–3 個本章選 A 不選 B 的決定,以及後來踩到
   的後果。
6. **Exercises** — 2–4 題,從「重現」到「擴充」到「診斷這個
   regression」。

### Part I — Foundations(3 章)

**Ch 1. 為什麼是一顆 GPU?這顆裡面有什麼?**

- 範圍凍結:GLES 2.0 + 4× MSAA、TBDR + unified SIMT、1 px / cycle
  peak。為什麼選這個 scope 而不是 Vulkan / desktop GL / mobile-only。
- Block diagram(CP → VF → SC → PA → RS → PFO → TBF → RSV)用三個
  zoom level 畫。
- Repo tour:每塊住在哪、`docs/MASTER_PLAN.md` 承諾了什麼、
  `docs/regress_report.md` 量了什麼。

**Ch 2. ISA。**

- 64-bit instruction layout:ALU / FLOW / MEM。3-source ALU、5-bit
  GPR field(32 個 register)、2-bit src class(GPR / CONST /
  VARYING / IMM)。三個讀者會想擴但我們沒擴的地方,以及為什麼。
- Encoding 走讀:[`compiler/include/gpu_compiler/encoding.h`](../compiler/include/gpu_compiler/encoding.h)。
- Decisions:5-bit GPR vs 6-bit(選 5;後來在 Ch 13 補 bit);沒給
  s2 class field(選了不給,但 Ch 13 又補了 s2_neg)。
- Hands-on:組一個 triangle pass-through shader、disassemble、把
  bit pattern 讀出來。

**Ch 3. Simulation strategy。**

- 三層 reference 為什麼都要在:
  1. `sw_ref` — golden C++ pipeline,沒有 clock,truth 來源。
  2. SystemC LT — 同樣演算法,TLM `b_transport`,untimed。
  3. SystemC CA — cycle-accurate、`sc_signal` + `SC_CTHREAD`,是
     architecture 跟 RTL 之間的合約。
- "Scene record / replay" 的把戲 — 在 GL API 層攔一次,然後想換
  哪個 backend 重跑都行。
- Hands-on:三個 backend 跑同一顆三角形,把 bit-identical PPM 印
  出來給讀者看。

### Part II — Software reference(4 章)

**Ch 4. Vertex pipeline。** Vertex fetch、shader 用 vec4 ALU 跑、
primitive assembly、viewport transform。Backface culling 與 top-left
fill rule(Ch 8 的 rasterizer 在 quad 對角線雙重覆蓋時會回頭咬到
這條規則)。

**Ch 5. Rasterization。** Edge function、barycentric、用 double
做中間值(Sprint 44 的 `edge_fn` 故事 — 對角線 pixel 因為單精度
FMA 而錯掉,改成 double 就 collapse 到 exact zero)、4× MSAA +
per-sample stencil。

**Ch 6. Per-fragment ops。** Depth + stencil + blend + colorMask。
依照 scissor + colorMask 做 clear。Front/back-separate stencil。
為什麼 blend factor 是 sw_ref-shape 的 enum(`BF_*`)而不是直接吃
GL 常數。

**Ch 7. End-to-end:triangle 變 PPM。** 把 clear + 一顆 triangle 接
進 `sw_ref`、存 PPM、寫 regression test。第一張 checked-in 的 golden
image。從這章開始 `ctest` 變成每章的綠 baseline。

### Part III — Compiler chain(4 章)

**Ch 8. GLSL frontend、lexer + parser。** 從零寫,約 1500 行:
[`compiler/glsl/src/glsl.cpp`](../compiler/glsl/src/glsl.cpp)。手刻
recursive-descent parser;`parse_compare`、`parse_addsub`、
`parse_mul`、`parse_paren`。為什麼隨機 shader corpus 推著我們做了
十次 lexer / parser hardening(Sprint 47–48 的故事)。

**Ch 9. Codegen + GPR allocation。** `Operand` + `emit_alu` +
`alloc_gpr`。c-bank 當作 unified scalar pool。Sprint 56 的 c-bank
從 16 widen 到 32。Sprint 58 的 `int(x)` truncation + `const`-fold
怎麼一次關掉 18 個 random-shader fail。

**Ch 10. SPIR-V → ISA。** 從 IR 的角度看。為什麼有兩個 frontend
(自家 GLSL + 透過 glslang 走 SPIR-V),各自能解掉哪些問題。

**Ch 11. Compiler 在 stress 下:VK-GL-CTS basic_shader。** 走過 4 個
random-shader bug,順序就是當時診斷的順序:

1. setp 是 scalar,vec `==` 要 dot-reduce(Sprint 57)。
2. Output slot encoding 從 2 bit 擴到 3 bit(Sprint 58 — *這是 ISA
   change*,示範 cross-layer 改動會觸到哪幾個 layer)。
3. `int(x)` truncation(Sprint 58)。
4. Const-scalar fold + init-aliasing 解 GPR pressure(Sprint 58)。

### Part IV — Hardware modeling(4 章)

**Ch 12. SystemC TLM-LT primer。** 看懂本 repo 的 block 所需的
TLM-2.0 + `b_transport` 知識下限。`_lt` flavour-suffix convention。
`gpu_top_lt` 怎麼把 15 個 block 接成可跑的 model。

**Ch 13. LT 到 CA:一個 block 的細節。** 把 CP 從 LT 走到 CA,同一個
block 兩個 abstraction level,用 tb 證明它們對 transaction sequence
的看法一致。

**Ch 14. 整條 CA chain + adapters。** SC → PA → RS → PFO → TBF →
RSV。四個 single-buffered pointer-passing adapter。為什麼 adapter
獨立成 module(沒折進 producer / consumer)— 這樣 `tb` 才能在任一邊
塞 stub。

**Ch 15. Memory subsystem。** MMU + L2 + MC、sidebands、CSR + PMU。
這是書裡最深的一章 architecture,讀者趕進度可以跳過。

### Part V — Conformance and verification(3 章)

**Ch 16. Scene format + replay harness。** 為什麼自己定一個 JSON-ish
的 scene format,而不是直接 replay 抓到的 GL trace。`SceneOp` 怎麼
演化:BATCH → BATCH+CLEAR → +CLEAR_DEPTH/STENCIL → +BITMAP →
Sprint 60 CLEAR scissor + lane → Sprint 61 multi-varying + full
blend + per-batch viewport。

**Ch 17. VK-GL-CTS 透過 `gpu_glcompat`。** 把 dEQP-GLES2 接到我們的
software pipeline(Sprint 42)。把 conformance 從 0/2143 推到
2035/2143 — 這章只挑四個 ROI 最高的 fix(top-left fill rule、
separate-channel blend、depth-test gate on depth-write、constant-depth
quad bypass),不把 16 個 sprint 全列。

**Ch 18. SC parity track。** 用同一個 scene 餵 SC chain,跟 `sw_ref`
做 diff。Pipeline 的 atexit-dump 把戲。Sprint 59 sweep 暴出來的三個
follow-up gap(CLEAR scissor / blend state / varying capture)、
Sprint 60–61 怎麼把它們關掉。讀者離開這章時,
`tools/run_vkglcts_to_sc.py` 在他自己的機器上能跑。

### Part VI — Beyond(2 章 + appendices)

**Ch 19. Toward RTL。** 只 sketch:CA model 已經釘下來的(timing、
hop count、FIFO depth)、RTL 還要選的(microarchitecture 細節、
clock domain、async FIFO)。給一份 reading list。

**Ch 20. Toward a driver。** 只 sketch:Linux EGL / GLES 2.0 接哪、
kernel doorbell 長什麼樣、CSR map 怎麼變成 ioctl。給一份 reading list。

**Appendix A.** macOS 上 build + run(Intel / M-series 共用 exFAT
volume)。Per-arch venv stub。CLT libc++ shim。`SC_CPLUSPLUS=201703L`
ABI workaround。(這是「你想 build 結果不過,這是為什麼」的
appendix。)

**Appendix B.** 怎麼讀 regress report。`docs/regress_report.md` 跟
`out/sc_e2e_summary.md` 是哪些 script 產出來的、量什麼、什麼算
regression。

**Appendix C.** 我們會重新選的決定。一份「當時對、現在會選不一樣」
的 list。

## Pedagogical strategy

1. **Project before concepts。** 每章一開始就跑既有的東西,然後再
   拆解它怎麼運作。讀者 30 秒之內看到動機。

2. **Multi-abstraction parity 從第一天開始。** Ch 3 把 sw_ref / LT /
   CA 一起搬上桌、給三者跑同一顆 triangle 證明它們同意。從此後每個
   「X works」的宣告,都要在每個 layer 都成立 — 不一致的地方就是
   那一章的戲劇。

3. **Test 跟 prose 同等地位。** 書裡每個數字都配一條 one-liner 給
   讀者跑。如果 `tools/run_vkglcts_sweep.py` 有一天跑不出書裡引的
   數字,那是書錯了不是 test 錯了。

4. **Decision 要有版面。** 每章點 1–3 個非顯而易見的選擇(5-bit
   GPR、不給 s2 class、scene format vs trace format、deqp 用 atexit
   dump 等),寫出後來幾個月踩到了什麼。學生在教科書裡看不到、實際
   專案裡天天看到的就是這部分。

5. **Git log 是 bibliography 的一部分。** 每個 sprint commit 都是
   一段 vignette。書裡標 `git tag chN-end`,讀者 `git checkout
   ch11-end` 就會落在一個全綠、跟那一章宣告的數字對得起來的 build
   上。

## Logistics

**Code repo。** 本 repo 就是教科書的 reference implementation。會另
建一份 frozen mirror 帶 chapter tag,後續 sprint 不會把 chapter-N
state 漂掉。

**Build matrix。** macOS Intel + Apple Silicon(目前 dev env);
Linux GCC-15 + Clang-17(CI baseline)。Windows 不在書的 scope —
要跑的同學自己用 Linux container。

**估計頁數。** 約 600 頁 + 100 頁 appendix 跟習題解答。對教科書來說
偏厚,但大部分是注解過的 code listing,單頁概念密度中等。

**估計寫作工夫。** Part-time 大約一年。一章一章寫;每一章 draft
要過 ctest + sweep 對著 chapter-end tag 跑乾淨才放行。

**Format。** 單一 source 是 markdown,build 出兩個 target:印刷
(LaTeX) + 網站。Code block 連到 GitHub 特定 commit 的 blob URL,
讀者點「跳到這個 function」會落在穩定的位置。

## What this textbook is not

- **不是 graphics 教科書。** Real-time rendering 的數學只有一章
  (Ch 5)。要完整版的人請搭配 *Real-Time Rendering* 一起讀。
- **不是 SystemC 教科書。** Ch 12–15 教夠讀懂本 repo 的 block 用;
  完整 SystemC pedagogy 還是 *SystemC: From the Ground Up* 是 ref。
- **不是 RTL 教科書。** Ch 19 寫 handoff sketch。RTL 本身另一本書。
- **不是已經寫完的書。** 這份 plan 是 first commit。每一章從 outline
  + 既有 code 開始長,等到對應的 hands-on script 在乾淨 checkout
  上能重現該章引的數字才放行。
