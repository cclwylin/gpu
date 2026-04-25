# third_party/

第三方相依,以 git submodule 或 fetch-on-build 方式管理。
Phase 0 end 凍結版本。

## 必要(Phase 0 凍結)
| Lib | Version | 用途 | License |
|---|---|---|---|
| SystemC | 2.3.3 | Core | Apache-2.0 |
| UVM-SystemC | 1.0 beta5 | Block/system verif | Apache-2.0 |
| Verilator | 5.x | Lint + sim | LGPL / Artistic |
| Icarus Verilog | 12.0 | Alt sim | LGPL |
| cocotb | 1.8+ | Python tb | BSD-3 |
| DRAMSim3 | latest | DRAM timing model | MIT |
| glslang | latest | GLSL → SPIR-V | BSD-3 |
| SymbiYosys | latest | Formal | ISC |
| yosys | latest | Synth frontend (lint/formal) | ISC |

## 可選
| Lib | 用途 |
|---|---|
| verible | Verilog lint |
| SPIR-V-Tools | SPIR-V validate / optimize |
| benchmark | C++ microbench |
| GoogleTest | C++ unit test |

## Phase 0 任務
- 選擇並鎖定版本
- 建立 reproducible build(Docker image 或 nix flake)
- License inventory,確認與專案目標(若商用)相容
