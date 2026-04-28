# systemc/blocks/ — Per-Block SystemC Modules

每個 block 一個子目錄。目錄名是該 block 的全名(no abbrev),C++ class
名是同名 UpperCamelCase。

| Dir | LT class | CA class | Owner |
|---|---|---|---|
| [commandprocessor/](commandprocessor/) | `CommandProcessorLt` | `CommandProcessorCa` | E1 |
| [vertexfetch/](vertexfetch/) | `VertexFetchLt` | `VertexFetchCa` | E1 |
| [shadercore/](shadercore/) | `ShaderCoreLt` | `ShaderCoreCa` | E1 |
| [primitiveassembly/](primitiveassembly/) | `PrimitiveAssemblyLt` | `PrimitiveAssemblyCa` | E1 |
| [tilebinner/](tilebinner/) | — | `TileBinnerCa` | E2 |
| [rasterizer/](rasterizer/) | `RasterizerLt` | `RasterizerCa` | E2 |
| [textureunit/](textureunit/) | `TextureUnitLt` | `TextureUnitCa` | E2 |
| [perfragmentops/](perfragmentops/) | `PerFragmentOpsLt` | `PerFragmentOpsCa` | E2 |
| [tilebuffer/](tilebuffer/) | `TileBufferLt` | `TileBufferCa` | E2 |
| [resolveunit/](resolveunit/) | `ResolveUnitLt` | `ResolveUnitCa` | E2 |
| [memorymanagementunit/](memorymanagementunit/) | `MemoryManagementUnitLt` | `MemoryManagementUnitCa` | E1 |
| [l2cache/](l2cache/) | `L2CacheLt` | `L2CacheCa` | E1 |
| [memorycontroller/](memorycontroller/) | `MemoryControllerLt` | `MemoryControllerCa` | E1 |
| [controlstatusregister/](controlstatusregister/) | `ControlStatusRegisterLt` | `ControlStatusRegisterCa` | E3 |
| [perfmonitorunit/](perfmonitorunit/) | `PerfMonitorUnitLt` | `PerfMonitorUnitCa` | E3 |

## 每個 block 子目錄結構(建議)
```
<blockname>/
  README.md              role, interface, status
  include/gpu_systemc/<blockname>.h
  src/<blockname>.cpp
  tb/                    block-level UVM-SystemC testbench
```

## 命名
- 目錄 / header / source 檔名:lowercase concatenated 全名(`commandprocessor`)
- C++ class:同名 UpperCamelCase(`CommandProcessorLt`)
- Port:`*_i` / `*_o` suffix
