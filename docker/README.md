# docker/ — Reproducible Build Environment

Dockerfile 建出的 image 含本專案所有 build / test / lint 需要的工具,
與 [`third_party/versions.yaml`](../third_party/versions.yaml) 鎖定的版本對齊。

## Build

```bash
docker build -f docker/Dockerfile -t gpu-dev:v1 .
```

首次建置約 15–30 分鐘(主要 yosys / verilator / systemc 要 compile)。

## Run

```bash
docker run --rm -it \
  -v "$PWD":/work -w /work \
  gpu-dev:v1 bash
```

Mount workspace 進 `/work`,host 檔案共用。

### 帶 GUI(optional,for waveform)
```bash
xhost +local:docker
docker run --rm -it \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v "$PWD":/work -w /work \
  gpu-dev:v1 bash
```

## 包含工具

| Tool | Version | Source |
|---|---|---|
| GCC | 11.4 | apt |
| Clang | 14 | apt |
| CMake | 3.22 | apt |
| Python | 3.10 | apt |
| Verilator | 5.020 | 原始碼編譯 |
| Icarus | 12.0 | 原始碼編譯 |
| Yosys + SymbiYosys | 0.38 | 原始碼編譯 |
| SystemC | 2.3.3 | 原始碼編譯 → `/opt/systemc` |
| UVM-SystemC | 1.0-beta5 | 原始碼編譯 → `/opt/uvm-systemc` |
| DRAMsim3 | pinned | build-tree at `/opt/DRAMsim3` |
| glslang | 14.0.0 | 原始碼編譯 |
| Python deps | `requirements.txt` | pip |

## CI 使用

CI(`.github/workflows/ci.yml`)流程:

1. **`dev-image` job**:每次 run 跑 `docker buildx build` with GHA cache,
   tag 兩個:`ghcr.io/<owner>/<repo>/dev:<sha>` + `:latest`,push 到 GHCR
2. **下游 jobs**(lint / rtl-lint / build / gen-check):用 `container:` 直接進 dev image,
   credentials 走 `GITHUB_TOKEN`(無需手動建 PAT)

### 加速效果

| 情境 | 耗時 |
|---|---|
| 第一次 run(cold cache) | ~25–30 min(完整 build) |
| Dockerfile 沒改 | ~30–60 sec(cache hit + push tag) |
| Dockerfile 改一點(e.g. 加 apt 套件) | ~3–10 min(局部 layer rebuild) |

### Image visibility

第一次 push 後,GHCR 上的 package 預設是 private。
這沒關係 — 同 repo 內的 workflow 用 `GITHUB_TOKEN` 自動可拉。
若要外部可見:GitHub → Profile → Packages → 點 image → Package settings → Change visibility → Public。

## 環境變數

Runtime 自動設:
- `SYSTEMC_HOME=/opt/systemc`
- `UVM_SYSTEMC_HOME=/opt/uvm-systemc`
- `DRAMSIM3_HOME=/opt/DRAMsim3`
- `LD_LIBRARY_PATH` 已含 SystemC + UVM 的 lib path

## 升版流程

1. 改 [`third_party/versions.yaml`](../third_party/versions.yaml)
2. 對應改 Dockerfile 的 ARG
3. `docker build ...` 驗 local 能過
4. PR → CI rebuild → 跑 regression → merge
5. Tag image:`gpu-dev:vN`

## Nix flake(未來)

暫不做;Docker 已滿足 reproducibility 需求。若未來需要更輕量 local dev
可加 `flake.nix`。
