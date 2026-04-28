# Ch 1 — 為什麼是一顆 GPU?這顆裡面有什麼?

> 6-section outline。讀者剛打開書、剛 clone 完 repo,還沒寫過任何
> shader。這章把 **scope、block diagram、layering、repo 目錄** 一次
> 鋪平,讓 Ch 2 講 ISA 時讀者已經知道 ISA 在整個 stack 哪一層、為
> 什麼長那樣。

## Goal

讓讀者在 30 分鐘內 **跑出綠 ctest、看到 95 % VK-GL-CTS sweep 結果、
在腦中放下整個 stack 的 block 圖**。離開這章時,後面 19 章每一章開
頭講「這章在 stack 哪一層」時,讀者不需要翻書就知道在哪。

## Concepts

讀者要帶走的 5 個 idea:

1. **Scope freeze 是 design discipline、不是限制** — GLES 2.0 +
   4× MSAA + TBDR + unified SIMT + 1 GHz @ 28 nm。Vulkan 沒進、
   desktop GL 沒進、compute shader 沒進、tessellation / geometry
   shader 沒進。這個 scope 小到 28 個月可以做完、conformance
   suite(VK-GL-CTS GLES2)既存可比、mobile-class GPU IP 業界仍在
   出貨 — 有 reference 也有意義。
2. **Pipeline = 11 個 block 接成的 chain** — CP → VF → SC → PA →
   RS → PFO → TBF → RSV(主路徑 8 個)+ TMU、MMU、L2、MC、CSR、
   PMU、BIN(輔助 7 個)。每個 block 對應 `systemc/blocks/<block>/`
   一個 dir、`docs/microarch/<block>.md` 一份規格。
3. **Driver + Compiler + Backend** — Application 透過 GL ES 2.0
   API 進 glcompat;glcompat 同時擔任 driver(state machine、
   dispatch)跟 compiler 的 host(GLSL → ISA);最後把 ISA bytecode
   + DrawState 餵給三個 sibling backend 之一(sw_ref / SystemC LT /
   SystemC CA),或寫成 plain-text scene file 之後再重播。三個
   backend 是並聯不是串聯。
   - **「ISA bytecode + DrawState + buffers」不是檔案** — 是
     in-process 的 C++ struct(`BakedProgram::vs_code/fs_code` 是
     `std::vector<uint64_t>`、`s.ctx.draw` 是 `DrawState`、buffers
     在 `glcompat::state()` 裡)。三個 backend 跟 driver 同一個
     process,直接 pointer / reference 餵下去,不落盤。
   - **`.scene` file 是另一條 path、且不對等** — scene 記的是
     **VS 已經跑完的快照**(per-vertex clip-space pos +
     varying[0..N-1])加 DrawState,**沒有 ISA bytecode**。設計理由:
     ISA encoding 之後改了(Sprint 56 c-bank、Sprint 58 output
     widen)舊 scene 仍能播;scene replay 結果若跟 sw_ref 不同,
     bug 一定在 PA/RS/PFO/TBF 而不在 VS,debug 搜尋空間先縮一個
     dimension。
4. **三份 doc + 一份 report** 是 plan-vs-actual 追蹤的全部:
   - [`docs/MASTER_PLAN.md`](../docs/MASTER_PLAN.md) — 28 個月承諾,
     單一真實來源。
   - [`docs/PROGRESS.md`](../docs/PROGRESS.md) — 開放的 to-do,後寫
     的 sprint 從這裡 sub。
   - [`docs/regress_report.md`](../docs/regress_report.md) — 數字
     的合約,哪一個 sweep / ctest pass / fail 各是多少。
   - [`docs/arch_spec.md`](../docs/arch_spec.md) + [`docs/isa_spec.md`](../docs/isa_spec.md)
     — frozen spec,後面所有 chapter 都從這兩份回頭引。
5. **這顆 GPU 是 textbook target、不是流片目標** — 1 GHz @ 28 nm 是
   pedagogical placeholder,跟真正流片的 PPA 沒對齊。讀者離開書時
   應該能流暢回答「我學的是 GPU 怎麼設計」而不是「我在書裡學會了
   tape-out」。

## Code walk

### 1.1 — 三張 block diagram

從最大顆到最細看一次:

**Zoom 1:Driver / Compiler / Backend(三層)**

```
┌──────────────────────────────────────────────────────┐
│ Application                                          │
│   glmark2 scene / VK-GL-CTS case / 手刻 .scene file  │
└────────────────┬─────────────────────────────────────┘
                 │ OpenGL ES 2.0 API
                 ▼
┌──────────────────────────────────────────────────────┐
│ glcompat   ← driver(runtime)+ compiler host         │
│  ├─ state machine                                    │ glcompat/src/glcompat_state.cpp
│  ├─ run_draw                                         │ glcompat/src/glcompat_es2.cpp
│  ├─ GLSL → ISA frontend                              │ compiler/glsl
│  ├─ GLSL → SPIR-V(opt-in)→ ISA                       │ compiler/glslang + compiler/spv
│  └─ scene capture(optional)                          │ glcompat/src/glcompat_render.cpp
└────────────────┬─────────────────────────────────────┘
                 │ in-process pointer-pass:
                 │   ISA bytecode (vector<uint64_t>) +
                 │   DrawState struct + GL buffers
       ┌─────────┼─────────┐
       ▼         ▼         ▼
   ┌────────┐ ┌────────┐ ┌────────┐
   │ sw_ref │ │ SC LT  │ │ SC CA  │
   │ truth  │ │ b_xact │ │ cycle  │
   └────────┘ └────────┘ └────────┘
                 │
                 │ optional fork:GLCOMPAT_SCENE 開啟時
                 │   plain-text scene file(post-VS 快照,
                 │   不帶 bytecode,只有 clip-space pos +
                 │   varying[] + DrawState)
                 ▼
            ┌──────────┐
            │ .scene   │
            └────┬─────┘
                 │
       ┌─────────┴─────────┐
       ▼                   ▼
   scene_runner        sc_pattern_runner
   (→ sw_ref)          (→ CA chain)
```

兩條箭頭職責不同:**上方** 是 driver 直接餵 backend(三個都拿到
還沒跑的 VS / FS bytecode + 原始 vertex attribute,得自己跑 VS);
**下方** 是 scene file,driver 已經 sw-side 跑完 VS,只把 post-VS
clip-space + varying 寫成 plain text,後面 backend 重播時 VS 是
hardcoded `mov o, c` 把 c-bank 搬到 output。後者 scene 不對 ISA
version 敏感,Sprint 60/61 的 ISA encoding bump 都沒讓既存 scene
失效。

**Zoom 2:Backend 內部 = 11-block pipeline**

```
CP ─► VF ─► SC ─► PA ─► RS ─► PFO ─► TBF ─► RSV ─► framebuffer
            ▲           │     ▲              ▲
            │           ▼     │              │
            └─── TMU ◄──┘   memory: MMU / L2 / MC
            CSR / PMU / BIN(sidebands)
```

8 條主路徑、7 個輔助 block。每個都是 `systemc/blocks/<name>/`。

**Zoom 3:單一 block 的 LT vs CA**

舉 `systemc/blocks/perfragmentops/` 為例:

```
perfragmentops/
├── include/gpu_systemc/perfragmentops_lt.h    ← LT,b_transport,呼叫 sw_ref
├── include/gpu_systemc/perfragmentops_ca.h    ← CA,sc_signal + SC_CTHREAD
└── src/
    ├── perfragmentops_lt.cpp                  ← 30 行,turtle 包裝
    └── perfragmentops_ca.cpp                  ← ~150 行,FSM
```

LT 直接 call `gpu::pipeline::per_fragment_ops`(sw_ref 那支函式)—
LT 的 algorithm = sw_ref 的 algorithm,沒重寫。CA 自己寫 FSM,但
最終 algorithmic decision 也呼回 sw_ref。三個 layer 對於「per-fragment
ops 該做什麼」這件事永遠 by-construction 同意;不同的只有
「什麼時候做」。

### 1.2 — Repo tour

```
.
├── docs/                  ← spec + plan + 量出來的數字
│   ├── MASTER_PLAN.md     ← 28 個月承諾
│   ├── PROGRESS.md        ← 待辦 to-do 清單
│   ├── regress_report.md  ← VK-GL-CTS / ctest 量到的數字
│   ├── arch_spec.md       ← 頂層 block 圖 + 資料流
│   ├── isa_spec.md        ← Shader ISA(64-bit instruction)
│   └── microarch/         ← 15 個 block 各一份規格
│
├── compiler/              ← ISA + assembler + simulator + GLSL/SPIR-V frontend
│   ├── include/gpu_compiler/
│   │   ├── encoding.h     ← 64-bit instruction layout(Ch 2 主場)
│   │   └── sim.h          ← ThreadState 合約
│   ├── assembler/         ← gpu-asm + gpu-disasm
│   ├── isa_sim/           ← gpu-isa-sim(+ warp 版)
│   ├── glsl/              ← GLSL → ISA 自家 frontend(Ch 8、Ch 9)
│   ├── glslang/           ← glslang FetchContent + GLSL → SPIR-V
│   └── spv/               ← SPIR-V → ISA lowering(Ch 10)
│
├── sw_ref/                ← golden C++ pipeline = truth(Part II)
│   ├── include/gpu/
│   └── src/pipeline/      ← 8 個 stage 各一個 .cpp
│
├── systemc/               ← 15 個 block,每個 LT + CA 兩種 flavour(Part IV)
│   ├── blocks/<name>/     ← 一個 block 一個 dir
│   ├── blocks/adapters/   ← 4 個 single-buffer pointer-passing adapter
│   ├── common/include/    ← TLM payload(ShaderJob、PrimAssemblyJob、…)
│   ├── tb/                ← 22 個 ctest entry
│   └── top/               ← gpu_top_lt + gpu_top_ca,把 15 個 block 接起來
│
├── glcompat/              ← GL ES 2.0 driver + scene capture
│   ├── src/glcompat_state.cpp        ← state machine(glClear、glScissor、…)
│   ├── src/glcompat_es2.cpp          ← extern "C" gl* + run_draw
│   ├── src/glcompat_render.cpp       ← scene capture / save_scene_impl
│   └── src/glcompat_runtime.h        ← 對 sw_ref / SC 的 export 介面
│
├── tests/
│   ├── scenes/            ← 手刻 .scene file + golden PPM
│   ├── conformance/       ← scene_runner(→ sw_ref)+ sc_pattern_runner(→ CA)
│   ├── glmark2_runner/    ← 6 個 GLES 2.0 場景(Ch 7、Ch 18 hands-on)
│   └── VK-GL-CTS/         ← Khronos CTS 上游 + 我們的 glcompat target shim
│
├── tools/
│   ├── setup_env.sh           ← per-arch venv + brew prefix(Appendix A)
│   ├── build_vkglcts.sh       ← out-of-tree CTS build
│   ├── run_vkglcts.py         ← 單組 sweep
│   ├── run_vkglcts_sweep.py   ← 11 組全 sweep → out/vkglcts_sweep.md
│   ├── run_vkglcts_to_sc.py   ← E2E:sw_ref ↔ SC chain diff(Ch 18)
│   └── aggregate_sc_e2e.py    ← TSV → markdown 摘要
│
├── ci/                    ← Linux Docker CI scripts
├── docker/                ← 上游 dev image
├── specs/                 ← machine-readable YAML(register_map、isa)
└── third_party/           ← versions.yaml(SystemC、glslang、glm 等版本鎖)
```

讀者讀到這裡應該能 **指著任何一個 sprint commit message,直接知道
那個 sprint 動到 repo 哪幾個 dir**。後面所有章節都會在 "Code walk"
段落引這份地圖。

### 1.3 — 三份 doc 的角色

打開三份 doc 各看一段:

- [`docs/MASTER_PLAN.md`](../docs/MASTER_PLAN.md) — 看
  Phase 0..3 的 timeline 跟 deliverable list。Phase 0 是 architecture
  + ISA freeze、Phase 1 是「sw_ref + compiler + SystemC LT 三軌
  parallel」、Phase 2 是「全 15 個 block CA」、Phase 3 是 RTL +
  driver。
- [`docs/PROGRESS.md`](../docs/PROGRESS.md) — 不到 100 行,是純
  to-do list。歷史 sprint 不在這裡(歷史在 regress_report 跟 git
  log),這份只列待辦。
- [`docs/regress_report.md`](../docs/regress_report.md) — 量出來的
  數字。VK-GL-CTS 11 組各自的 pass / fail / skip,每個 sprint 怎麼
  動了哪幾個 group。

讀者看完應該能回答:「我現在 checkout `main`,我在 Phase 幾?哪些
事情已經做完?哪些還沒?」這三份 doc 加 git log 就能回答。

## Hands-on

```sh
git clone <repo> gpu && cd gpu

# (1) 一次性 prereq(每台 mac 各做一次)
xcode-select --install
brew install cmake ninja libpng
./tools/setup_env.sh --bootstrap-venv     # 建 ~/.local/share/gpu-venv/$(uname -m)
source .venv/activate

# (2) 主 build + 全綠 ctest
./tools/setup_env.sh --configure          # cmake -G Ninja → build-$(uname -m)
cmake --build "$BUILD_DIR" -j
ctest --test-dir "$BUILD_DIR" -j4         # 應該 100 % pass(56/56 with -DGPU_BUILD_SYSTEMC=ON)

# (3) VK-GL-CTS sweep,讀 95 % 那個數字
./tools/build_vkglcts.sh                  # out-of-tree build deqp-gles2(~5 分鐘第一次)
python tools/run_vkglcts_sweep.py \
       --bin "$VKGLCTS_BUILD_DIR/modules/gles2/deqp-gles2"
# 印 "total:  2035/2143 pass (95.0%)" 左右

# (4) 開一份 .scene 看看 driver 到 backend 的 boundary 長什麼樣
cat tests/scenes/alpha_blend.scene | head -30
```

讀者跑完看到三件事:`ctest` 全綠、sweep 印出 ~95 % 的數字、scene
file 是人讀得懂的 plain text。如果這三件事在乾淨 checkout 上不成
立,就是這章寫錯了 — 不是讀者的環境壞掉。

## Decisions log

| 決定 | 走 A 不走 B 的原因 | 後來踩到 |
|---|---|---|
| **Scope = GLES 2.0 + 4× MSAA**(不是 Vulkan / 不是 desktop GL / 不是 compute) | 三個原因疊起來:(a)spec 小到 28 個月做得完;(b)VK-GL-CTS GLES 2.0 已存在、可量;(c)mobile-class IP 業界還在出 — 有真實 reference | 從來沒踩到反悔的痛;但 Vulkan 教學使用者多,後來寫第二本書時可能會選 Vulkan baseline。 |
| **TBDR 不是 IMR** | 業界 mobile-class 標準作法;tile size 16 × 16 + tile binner + tile buffer + resolve unit,讓 memory bandwidth 壓到 IMR 的 1/4–1/3。教學意義也比 IMR 大(學到 tile binning + resolve 兩個獨特階段) | Phase 2 BIN block 寫起來比預期久;tile binning 裡的 conservative-coverage 邊界要寫 testbench 證明。 |
| **三層 simulation reference**(sw_ref / LT / CA)三條都做 | 一條做不夠 —「algorithm 對不對」跟「timing 對不對」是兩個獨立問題,得分開 backend 才能定位 bug 在哪一層。Ch 3 整章解釋。 | Sprint 44 的 `edge_fn` `double` 化:sw_ref 跟 SC 都同時錯、修一個地方兩邊都對 — 這就是「sw_ref 真的是 truth、不是 SC 的橡皮章」的證明。 |
| **Driver + compiler 都在 glcompat 裡** | 業界 stack 一致(Mesa、ANGLE、proprietary 都這樣 — user-space driver call compiler at link time)。教學上也省一個 process boundary | Sprint 56–58 的 GPR pressure 故事:compiler 改 codegen → driver 重 link program → backend 跑 → 一條 commit 動三層,但因為三層都在 repo 裡,改起來是 single PR。 |
| **`PROGRESS.md` 只放 to-do、不放歷史** | 歷史在 `regress_report.md` § 各 Sprint + git log;PROGRESS 太長就沒人讀。書這次重寫從 1697 行壓到 111 行 | 這個 decision 本身就是書寫到 Sprint 61 才下的;之前 PROGRESS 變雜七雜八的 commit log。讀者要從這裡學到「定位文件的單一職責」。 |

## Exercises

1. **重現 hands-on 三個數字**(easy)。把你機器上的 ctest 結果、
   sweep total、`alpha_blend.scene` 的前 5 行貼出來。三個數字應該
   跟書印的一樣;不一樣就 file issue。
2. **讀 PLAN、定位現在在哪**(easy)。打開 `MASTER_PLAN.md`,找
   出「Phase 2 / Sprint 28」那一格描述;對照
   `docs/regress_report.md` 看你 checkout 的 commit 是 Sprint 幾。
   寫一句話:「我現在處在 Phase X,Phase Y 完成度 N %。」
3. **挑一個 block,找它的 dir**(easy–medium)。隨便挑 CP / VF /
   SC / PA / RS / PFO / TBF / RSV / TMU / MMU / L2 / MC / CSR /
   PMU / BIN 任一個,找到對應的 `systemc/blocks/<name>/` 跟
   `docs/microarch/<name>.md`。把 microarch doc 的「Open
   questions」清單列出來 — 那就是這個 block 後面 chapter 還會
   handle 的 follow-up。
4. **設計題:把 scope 改成 Vulkan**(open)。如果這顆 GPU 從第一天
   就要支援 Vulkan 1.0,scope freeze 那一章會多寫哪些東西、
   block 圖會多哪幾個 block、ISA 會多哪幾個 instruction format?
   不要超過一頁。

## 章節 anchor

| anchor | 內容 |
|---|---|
| `docs/MASTER_PLAN.md` | 28 個月承諾 |
| `docs/PROGRESS.md` | 待辦 to-do 清單(< 200 行) |
| `docs/regress_report.md` | 量出來的數字 |
| `docs/arch_spec.md` | 頂層 block 圖 + dataflow |
| `docs/isa_spec.md` | Shader ISA(Ch 2 主場) |
| `docs/microarch/` | 15 個 block 各一份規格 |
| `README.md` | macOS quickstart + per-arch venv stub |
| `tools/setup_env.sh` | env detection / configure / venv bootstrap |
| `tools/run_vkglcts_sweep.py` | sweep 主驅動程式 |
| ctest 全綠(macOS Intel + Apple Silicon) | hands-on green baseline |

## chapter-end tag

`git tag ch01-end` 落在「最近一次 ctest 全綠 + sweep ≥ 95 % pass」
的 commit。讀者 checkout 這個 tag,跑 hands-on 步驟 (1)–(4) 應該全
重現。
