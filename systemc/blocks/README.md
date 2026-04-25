# systemc/blocks/ — Per-Block SystemC Modules

每個 block 一個子目錄。目錄名是該 block 的全名(no abbrev),C++ class
名是同名 UpperCamelCase。

| Dir | Block class | Owner |
|---|---|---|
| [commandprocessor/](commandprocessor/) | `CommandProcessorLt` | E1 |
| [vertexfetch/](vertexfetch/) | `VertexFetchLt` | E1 |
| [shadercore/](shadercore/) | `ShaderCoreLt` | E1 |
| [primitiveassembly/](primitiveassembly/) | `PrimitiveAssemblyLt` | E1 |
| [tilebinner/](tilebinner/) | `TileBinner` | E2 |
| [rasterizer/](rasterizer/) | `RasterizerLt` | E2 |
| [textureunit/](textureunit/) | `TextureUnit` | E2 |
| [perfragmentops/](perfragmentops/) | `PerFragmentOps` | E2 |
| [tilebuffer/](tilebuffer/) | `TileBuffer` | E2 |
| [resolveunit/](resolveunit/) | `ResolveUnit` | E2 |
| [memorymanagementunit/](memorymanagementunit/) | `MemoryManagementUnit` | E1 |
| [l2cache/](l2cache/) | `L2Cache` | E1 |
| [memorycontroller/](memorycontroller/) | `MemoryController` | E1 |
| [controlstatusregister/](controlstatusregister/) | `ControlStatusRegister` | E3 |
| [perfmonitorunit/](perfmonitorunit/) | `PerfMonitorUnit` | E3 |

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
