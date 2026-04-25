# ci/ — Continuous Integration

## Structure
- [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) — GitHub Actions pipeline
- 本目錄下是被 CI 呼叫的 shell 腳本

## Scripts
| Script | 用途 |
|---|---|
| `check_clang_format.sh` | C++ / SystemC format check |
| `check_python.sh` | Python black + ruff + mypy |
| `check_specs.sh` | YAML validate(registers / isa) |
| `check_verilog.sh` | Verilator lint(Phase 3+) |
| `setup_deps.sh` | Install SystemC / third_party |
| `build.sh` | Build sw_ref / compiler / systemc |
| `smoke.sh` | Run smoke regression(10 scene) |
| `regen.sh` | 從 specs/ 重新生成 code,檢查 drift |
| `nightly.sh` | Full regression(500 shader + 50 scene) |

## Policy
- PR smoke:5 分鐘內
- Nightly full:1 小時內
- Coverage 報告上傳 dashboard
- Perf 趨勢上傳 dashboard

## Phase 0 stub
大部分 script 目前是 placeholder,等對應 code 存在才會生效。
