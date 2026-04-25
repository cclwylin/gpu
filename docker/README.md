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

GitHub Actions 可直接 `uses: docker://...` 或 pull 預 build 的 registry image。
Phase 0 會建 CI workflow 做 cached image build + push。

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
