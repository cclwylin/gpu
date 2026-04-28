# Ch 18 — SC parity track: VK-GL-CTS through the cycle-accurate chain

> 6-section outline。預設讀者讀過 Ch 12-15(SystemC LT → CA → 整條 chain
> + memory subsystem)+ Ch 16(scene format)+ Ch 17(VK-GL-CTS through
> `gpu_glcompat`)。這是書的最後一個 conformance 章 — 終於把 Part II
> 的 sw_ref、Part IV 的 SC chain、Part V 的 dEQP 三股縫起來,讓讀者
> 在自己機器上看到「同一個 dEQP case,sw_ref 跟 SC chain pixel-equal」。

## Goal

讓讀者在 90 分鐘內:

- 跑 `tools/run_vkglcts_to_sc.py` 在自己機器上看到 `bp=19 / 1985`
  之類的 bit-perfect count。
- 知道為什麼這個 19 不是「99 % 還沒打通」,而是「打通的這 19 個是
  Sprint 60 一路 land 的 strong baseline,接下來每個新關住的 group
  都是 chapter exercise 級別的小工作」。
- 看完 atexit-dump 那段把戲,在自己 toy project 想抓 GL trace 時
  懂得用同一招。

讀者離開這章時應該能回答:**為什麼 SC parity 走的是 scene format 而
不是 GL trace replay?** 答案在 Ch 16,但這章把 trade-off 真正落地
看到。

## Concepts

讀者要帶走的 5 個 idea:

1. **「pixel-equal」是強得令人害怕的 contract** — sw_ref 的 render
   是 4× MSAA + 浮點 pipeline,SC chain 是 cycle-accurate 整數
   coordinate + signal-level dance。這兩條 path 的中間值差很多,
   但最後 PPM byte 要逐 byte 對。能對得上,代表 scene format 涵蓋
   了所有影響輸出的 state — 任一個被遺漏的 state 都會在某 case 上
   出現 RMSE > 0。

2. **Atexit hook 是 dEQP 的 escape hatch** — dEQP 自己 manage
   process lifetime,沒有「test 結束時呼叫 X」的 explicit hook。
   但 `glcompat` 在 process atexit 時 dump 最後一個 captured scene
   到 `$GLCOMPAT_SCENE` env 指的 path — dEQP 結束時 atexit 跑、
   driver 拿到 scene、driver 餵 SC chain。整個 trick 的核心是
   **glcompat 收的不是 GL trace,是 batched scene**,所以 atexit
   的 single scene 就涵蓋整個 test。

3. **Bit-perfect 跟 RMSE ≤ ε 是兩個不同 metric** — 19 / 1985 是
   bit-perfect(每 pixel 完全一樣)。我們同時記 RMSE — 一些 case
   RMSE = 0.4(視覺等同但有兩個 pixel 差 1)。書裡的 sweep 用兩個
   threshold 量,讓讀者看「pass with strict bit equality」vs 「pass
   with perceptual equality」的分布。

4. **Three follow-up gaps from Sprint 59 sweep** — 第一輪打通(Sprint
   59)只有 14 / 2019 bit-perfect。後續三個 sprint(60 / 60-mid /
   61)各關掉一個明確的 gap:CLEAR scissor、blend state、varying
   capture。整個過程是 **看 RMSE histogram bimodal split → 抓最大
   一坨 case 的共同 root cause → fix 一條 → 重跑 sweep**。這是
   sweep-driven 工程的範本。

5. **SC chain 的 latency 是這個 metric 的隱含 cost** — 跑一次
   `sc_pattern_runner` 平均 6-15 秒(取決於 fragment 數)。1985
   case × 平均 8 秒 ≈ 4.4 小時。Workers parallelism + scene caching
   後降到 ~30-90 分鐘,但仍是「不能在 commit pre-hook 跑」的
   metric。書裡示範每週一輪 + commit-time 只跑 15 case smoke
   subset。

## Code walk

### 18.1 — Atexit hook 在 glcompat

[`glcompat/src/glcompat_state.cpp` 的 `atexit_dump_scene`](../glcompat/src/glcompat_state.cpp)
(Sprint 59 加的)。三段:

- `getenv("GLCOMPAT_SCENE")` 拿目標 path;若沒設就 return — driver
  沒在 capture 模式
- 把 process lifetime 累積的 `g_scene_ops`(per-batch state +
  vertex / varying 資料)寫成 textual scene format
- 註冊在 `init_glcompat()` 第一次被呼叫時 — `std::atexit(&dump)`

讀者要看的 detail:**為什麼 atexit 而不是 explicit `flush_scene()`
API?** 因為 dEQP 不肯被加 vendor-specific 呼叫 — 我們只能 hook 進
GL 自己的 entry point。Process exit 是唯一保證會跑的點。

### 18.2 — `sc_pattern_runner` 的 replay

[`tests/conformance/sc_pattern_runner.cpp`](../tests/conformance/sc_pattern_runner.cpp)。
Sprint 59 起改了大半,Sprint 60-61 跟著 scene format 一起 evolve。
四段:

- Scene parser:`parse_scene_file` 讀 textual format → `vector<SceneOp>`
- SC chain 構建:`make_chain()` 從 `gpu_top_ca` 跟它的 15 個 block
  + 4 個 adapter wire 起來
- Per-op replay:對 `BATCH` 重建 DrawState 餵 `sc_to_pa_adapter_ca`、
  對 `CLEAR` 直接呼 framebuffer clear、對 `BITMAP` 走 `glDrawPixels`-like
  CPU path
- PPM dump:跑完 chain 之後讀 framebuffer、寫 P6 PPM

**為什麼 PPM 不是 PNG?** 因為 P6 是純 byte 序列、好 hash、好 diff、
不需要 zlib。CTS 自己存 PNG,但我們 sweep 對 SC 直接用 PPM —
「pixel byte 序列相等」是這章的 contract,壓縮 format 引入的
chunk-level metadata 只是噪音。

### 18.3 — Driver:`run_vkglcts_to_sc.py`

[`tools/run_vkglcts_to_sc.py`](../tools/run_vkglcts_to_sc.py)。403 行
但 90 % 是 argparse 跟 TSV 寫。核心 path 在 `process_one_case`:

1. 用 `tempfile.NamedTemporaryFile` 開兩個 temp:`scene` 跟 `qpa`
2. `subprocess.run` 跑 `deqp-gles2 --deqp-case=<name>
   --deqp-log-filename=<qpa>`,env 加 `GLCOMPAT_SCENE=<scene>`
3. 從 `qpa` log 抽 `Result` image(base64 PNG)
4. `subprocess.run` 跑 `sc_pattern_runner <scene> <ppm>`
5. compare PNG vs PPM:per-channel RMSE / max-error / diff-pixel 數

**為什麼 driver 在 Python 不在 C++?** 因為 driver 的工作 80 % 是
process orchestration + TSV 累加 + resume — 這是 Python 的甜蜜點。
SC chain 跟 dEQP 是 native 的,跑得跟在 C++ driver 裡一樣快;driver
只在等。

### 18.4 — Scene format 三次 evolve(Sprint 60-61)

| 版本 | trigger | 加什麼 | bit-perfect 增量 |
|---|---|---|---:|
| Sprint 59 | 第一輪 sweep | base scene 格式 — vertex stream + per-batch state(depth / cull / blend simple) | 14 / 2019 |
| Sprint 60 | `color_clear` 17 / 19 fail histogram 集中在「scissored clear」+「masked clear」上 | `SceneOp::CLEAR` 帶 scissor rect + colorMask lane | +5 → 19 / 19 (完整 color_clear group) |
| Sprint 60-mid | blend RMSE bimodal — 一坨 RMSE=0、一坨 RMSE=110 | 完整 blend state(separate src/dst RGB/alpha factor + blend equation + blend color) | RMSE 大降:median 110 → 6.2(50-case sample) |
| Sprint 61 | 還有一坨 case 看 varying-2-onwards 顏色全錯 | multi-varying capture(VS 寫到 `o[1..7]` 的 vec4 也進 scene)+ per-batch viewport(`vp_x/y` 進 SceneOp + `ScToPaAdapterCa`) | 沒重跑 sweep 收尾,handoff 時是 `bp=19 still, RMSE distribution thinner` |

[`glcompat/src/glcompat_render.cpp`](../glcompat/src/glcompat_render.cpp)
是這四個 stage 的 batch capture path。每個 evolve 都對應一個明確的
field 進 `SceneOp` struct。

### 18.5 — Sweep result 怎麼讀

[`tools/aggregate_sc_e2e.py`](../tools/aggregate_sc_e2e.py) 把 TSV
變成 markdown table。三個 column 要看:

- **bp**:per-pixel exact match 的數
- **rmse_p50** / **rmse_p99**:50/99 percentile RMSE。`rmse_p50 = 0`
  表示一半 case 已經 bit-perfect、剩下一半在不同程度的 noise
- **note breakdown**:`sc timeout` / `no scene` / `crash` 各自佔比
  — 這是「沒被計入分母」的 case,ROI 要靠 timeout budget 跟 chain
  speed 來換

`--top-level` 旗標把 1985 case 收成 11 group / 一行,對 quick
status check 很有用。

## Hands-on

```sh
source .venv/activate
cmake --build build-arm64 --target sc_pattern_runner

# (1) 確認 sw_ref-pass 的 case 列表存在(從 sw_ref sweep 抽出)
python3 tools/run_vkglcts_sweep.py --groups dEQP-GLES2.functional.fragment_ops \
        --groups dEQP-GLES2.functional.color_clear \
        --groups dEQP-GLES2.functional.depth_stencil_clear \
        --groups dEQP-GLES2.functional.buffer
# 從 build_vkglcts-arm64/modules/gles2/TestResults.qpa 抽 Pass 的 case name
# 寫到 /tmp/sw_pass_cases.txt,一行一個

# (2) 跑 SC E2E sweep
python3 tools/run_vkglcts_to_sc.py \
    --cases-file /tmp/sw_pass_cases.txt \
    --workers 4 \
    --sc-timeout 15 \
    --tsv /tmp/sc_e2e.tsv \
    --resume

# 預期 ~30-90 分鐘(M4 mini 4 worker 大致是 50 分鐘)。
# 終端最後一行: total: bp=19 / 1985 (sample numbers — 跟 chapter-end tag
#   的對得起來;讀者跑出來不一樣表示 chain 哪邊長歪了)

# (3) 看單一 case
build-arm64/tests/conformance/sc_pattern_runner \
    /tmp/cts2sc-XXXX/w0/scene.scene /tmp/sc.ppm
# 對 sw_ref:從 qpa log 抽 Result PNG
# python3 tools/aggregate_sc_e2e.py /tmp/sc_e2e.tsv --top-level
#  → 每 group 一行,pass / total / median RMSE / bit-perfect %

# (4) 重現 Sprint 60 的 CLEAR scissor fix(checkout chapter mid-tag)
git checkout ch18-clear-scissor-pre
cmake --build build-arm64 --target sc_pattern_runner
python3 tools/run_vkglcts_to_sc.py \
    --group color_clear --workers 1 --sc-timeout 15
# 應該 8/19 bit-perfect

git checkout ch18-end
cmake --build build-arm64 --target sc_pattern_runner
python3 tools/run_vkglcts_to_sc.py \
    --group color_clear --workers 1 --sc-timeout 15
# 應該 19/19 bit-perfect
```

讀者跑完看到 19 / 19 + sweep 數字跟 `out/sc_e2e_summary.md` 對得起來,
這章就站住了。

## Decisions log

| 決定 | 走 A 不走 B 的原因 | 後來踩到 |
|---|---|---|
| **Scene format 自己定,不 replay GL trace** | GL trace 是 ~50 個 entry point 一次性的 noise(buffer upload、texture upload、swap chain etc.);scene 是「畫這個三角形 with this state」的精煉。對 SC chain 友善太多 | Sprint 60-61 三次擴容(CLEAR、blend、multi-varying)都是「scene format 漏掉一個 state」的故事。讀者在自己 project 用同樣手法時要從第一天就規劃 evolve 機制 — 我們的 textual format 加 field 不會 break parser 是運氣好 |
| **Atexit hook,不加 explicit dump API** | dEQP 不肯被加 vendor-specific 呼叫 | Atexit 在 deqp 用 `_exit` 強退時不 fire — 還好 deqp-gles2 是正常 `exit()`。如果以後 driver 換成有 abort path 的 framework,要找另一個 hook 點 |
| **PPM 不 PNG 在 SC 端 output** | byte 序列直接 hash,沒 zlib chunk noise | Diff tool 要寫兩條 path(PNG decode + PPM read)— 多 30 行 driver code,但跨整個 sweep 換來 deterministic byte-equal contract |
| **Driver 在 Python** | Process orchestration / TSV / resume 對 Python 是甜蜜點 | C++ 那邊有些 dEQP 自有的 case-filter helper 用不到 — 我們重新 implement parsing。書裡提一句,給想 port to C++ 的讀者一個入口 |
| **15 秒 SC timeout** | 4.4 小時 / 1985 case ÷ 4 worker = 約 33 分鐘理想值,留 50 % overhead;timeout 太短會把 large fragment count 的 case 全 reject | 上 `buffer.write` 那種 random buffer 大尺寸 case 會 ~50 % timeout。Sprint 62 的 follow-up(沒進 handoff)是 per-group adaptive timeout |

## Exercises

1. **重現 19/19**(easy)。Hands-on 跑完。如果讀者 bp 數字 ≠ 19,
   貼 sweep TSV 跟 `aggregate_sc_e2e.py --top-level` 出來的 markdown,
   到 issue tracker — 這是 chapter regression。

2. **加一個 SceneOp**(medium)。`SceneOp::CLEAR_DEPTH_STENCIL` 現在
   是 `CLEAR + clear_depth + clear_stencil` 的三合一 op。改成兩個
   separate op(`CLEAR_DEPTH` / `CLEAR_STENCIL`),確保 scene format
   parser + sc_pattern_runner 都跟著動,跑 sweep 證明沒 regress。

3. **量 timeout sensitivity**(medium)。把 `--sc-timeout` 從 15 拉到
   30,跑同一個 sweep,看 `note=sc timeout` 那欄少多少。算一個
   cost / coverage curve 表。

4. **設計題:precomputed scene cache**(open)。每次 sweep 重跑都
   re-deqp,但其實 deqp 的輸出在 ch18-end commit 上是 deterministic
   的。設計一個 `--scene-cache <dir>` 模式:第一次跑 dump scene 到
   cache、後續直接 read。算 best-case 跟 worst-case speedup;列出
   兩個會讓 cache invalidate 的 commit pattern。

5. **把 SC chain 換掉**(open + 進 RTL)。Ch 19 的 sketch 提到 RTL
   blueprint。如果讀者真要實作 RTL 版,把 `sc_pattern_runner` 的
   chain 換成 RTL simulator(Verilator / Icarus / 商用 simulator),
   sweep 應該保持同樣的 19 / 1985 bit-perfect。算這要多動什麼
   harness — 跟 RTL 自己的工作量比起來小得多。

## chapter-end tag

`git tag ch18-end` 落在這個 commit:**`40d3e78`** + Sprint 59-61 的
三批進度。讀者 checkout 跑 hands-on (1)–(3) 應該得到:

- `bp=19 / 1985`(色組總數可能微幅波動,timeout 緊的 case)
- `color_clear 19/19`、`depth_stencil_clear 11/11`(在 sw_ref 端
  pass 的 group 都 100 % feed scene)
- aggregate top-level table 上 `fragment_ops` 那行 RMSE p50 為個位數

跟這個數字對不上的話,先檢查 `ls /opt/homebrew/lib/libsystemc.dylib`
是不是 brew 的 3.0.x — Sprint 62 的 cmake fallback 是抓那條 path,
換 install location 要改 [`systemc/CMakeLists.txt`](../systemc/CMakeLists.txt)。

## 章節 anchor

| anchor | 內容 |
|---|---|
| [`glcompat/src/glcompat_state.cpp`](../glcompat/src/glcompat_state.cpp) | atexit dump |
| [`glcompat/src/glcompat_render.cpp`](../glcompat/src/glcompat_render.cpp) | scene capture path,Sprint 59-61 evolve 都在這 |
| [`tests/conformance/sc_pattern_runner.cpp`](../tests/conformance/sc_pattern_runner.cpp) | scene parser + SC chain replay |
| [`tools/run_vkglcts_to_sc.py`](../tools/run_vkglcts_to_sc.py) | E2E driver(deqp + chain + diff)|
| [`tools/aggregate_sc_e2e.py`](../tools/aggregate_sc_e2e.py) | TSV → markdown |
| [`systemc/CMakeLists.txt`](../systemc/CMakeLists.txt) | Sprint 62 brew fallback(找不到 cmake config 時用 manual import target)|
| [`systemc/blocks/adapters/include/gpu_systemc/sc_to_pa_adapter_ca.h`](../systemc/blocks/adapters/include/gpu_systemc/sc_to_pa_adapter_ca.h) | viewport `vp_x/y` 從 scene 進 chain 的 entry point(Sprint 61) |
| `out/sc_e2e_summary.md` | aggregator 產的 markdown(讀者跑完會新生)|
| `tests/conformance/sc_pattern_runner` ctest entry | smoke level |
