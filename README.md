# GPU Project — OpenGL ES 2.0 Programmable GPU (RTL Target)

一個完整的 GPU hardware 實作專案,從 OpenGL simulation → SystemC cycle-accurate
model → synthesizable RTL,支援 OpenGL ES 2.0 programmable pipeline + 4× MSAA。

## Target

| 項目 | 規格 |
|---|---|
| **API** | OpenGL ES 2.0 (full) + 4× MSAA + alpha-to-coverage |
| **Architecture** | TBDR (Tile-Based Deferred Rendering) + Unified SIMT Shader |
| **Tile Size** | 16 × 16 pixel |
| **Warp** | 16 threads (4 lane × 4 thread) |
| **Frequency** | 1 GHz @ 28nm / 500 MHz FPGA |
| **Peak** | 1 pixel/cycle (1×), ~0.8 pixel/cycle (4× MSAA) |
| **Bus** | AXI4 (2× 128-bit master) + APB (CSR) |
| **Delivery** | Synthesizable Verilog RTL, tape-out ready |
| **Timeline** | 28 months, 2–3 FTE |

## Status

**Phase 0 — Architecture Freeze** (kick-off)

## 專案結構

```
docs/            規格文件(single source of truth)
specs/           機器可讀 spec(YAML,自動生 header/SystemC/RTL/doc)
sw_ref/          C++ OpenGL ES 2.0 reference model (golden)
compiler/        GLSL → SPIR-V → IR → shader ISA
systemc/         SystemC TLM + cycle-accurate model
rtl/             Verilog RTL (Phase 3)
driver/          Linux EGL + GLES 2.0 driver
tests/           Scene corpus + shader corpus + conformance
tools/           Trace diff / scene gen / regmap gen
third_party/     SystemC / DRAMSim3 / glslang
ci/              CI scripts + coding style check
```

## 關鍵文件

- [Master Plan](docs/MASTER_PLAN.md) — 28 個月總規劃,單一真實來源
- [PROGRESS](docs/PROGRESS.md) — 已交付項、目前狀態、open follow-ups
- [Architecture Spec](docs/arch_spec.md) — 頂層 block 圖與資料流
- [ISA Spec](docs/isa_spec.md) — Shader instruction set
- [MSAA Spec](docs/msaa_spec.md) — 4× MSAA 實作細節
- [Coding Style](docs/coding_style.md) — C++/SystemC/Verilog 規範
- [Register Map](specs/registers.yaml) — CSR 定義
- [ISA YAML](specs/isa.yaml) — 機器可讀 ISA

## Quick Start

### macOS (Intel iMac + Mac mini M4 共用同一顆 exFAT 外接硬碟)

這個 repo 設計成兩台 Mac 可以共掛同一顆磁碟同時開發。CMake cache 會把 host
arch + 絕對 compiler 路徑寫死到 build dir,所以**build dir 一定要每台一份**。

一次性 prereq(每台機器各做一次):

```sh
xcode-select --install                 # Apple clang + libc++ + macOS SDK
brew install cmake ninja libpng        # /usr/local on Intel, /opt/homebrew on M*
./tools/setup_env.sh --bootstrap-venv  # 建 ~/.local/share/gpu-venv/$(uname -m)
```

`.venv/activate` 是 dispatcher stub(注意路徑是 `.venv/activate` 不是
標準的 `.venv/bin/activate` — 這個 repo 用簡短形式)。`source` 它會偵
測 `uname -m` 自動 source 對應的 `~/.local/share/gpu-venv/<arch>/`。
venv 本體不能放 exFAT 上,macOS 會在 `site-packages/` 裡寫 `._*`
shadow → Python 載入時當 `.pth` 解析爆 UTF-8。

Configure + build(兩台都用同一條指令):

```sh
source tools/setup_env.sh              # 自動 export BUILD_DIR=build-$(uname -m)
cmake -G Ninja -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j
ctest --test-dir "$BUILD_DIR" --output-on-failure
```

或一行 configure:

```sh
./tools/setup_env.sh --configure
```

| 主機 | BUILD_DIR | brew prefix |
|---|---|---|
| Intel iMac (x86_64) | `build-x86_64/` | `/usr/local` |
| Mac mini M4 (arm64) | `build-arm64/` | `/opt/homebrew` |

`build-*/` 跟 `build_vkglcts-*/` 都在 `.gitignore`,兩台不會互相干擾。

VK-GL-CTS optional gate(Sprint 42)同樣 arch-aware,`tools/build_vkglcts.sh`
會自動用對應 `build_vkglcts-$(uname -m)/`。

### Linux / CI

CI 走 Docker(`docker/Dockerfile` + `ci/build.sh`),用傳統的 `build/`。
詳見 [`.github/workflows/ci.yml`](.github/workflows/ci.yml)。

### Troubleshooting

- **`fatal error: 'cstdint' file not found`** — Apple CLT 有時會遺失 libc++
  headers(只剩 ~12 個檔)。CMake 偵測到後會自動從現用 SDK 注入,configure
  log 會印 `macOS libc++ shim: -isystem .../usr/include/c++/v1`。要根治請
  `sudo rm -rf /Library/Developer/CommandLineTools && sudo xcode-select --install`。
- **exFAT volume `._*` 檔案** — macOS 在非 HFS 卷上會生 AppleDouble
  shadow,已在 `.gitignore` 排除;若 `find` / glob 仍掃到,跑
  `dot_clean /Volumes/<vol>` 清掉。
- **`.sh` 沒執行權限** — exFAT 不保留 unix exec bit;新 clone 後跑
  `chmod +x tools/*.sh ci/*.sh`。

## Contribution

- 所有 spec 變更走 PR review
- CI 綠才能 merge
- 詳見 [`docs/coding_style.md`](docs/coding_style.md)
