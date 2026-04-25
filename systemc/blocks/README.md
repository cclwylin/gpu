# systemc/blocks/ — Per-Block SystemC Modules

每個 block 一個子目錄。

| Dir | Block | Owner |
|---|---|---|
| [cp/](cp/) | Command Processor | E1 |
| [vf/](vf/) | Vertex Fetch | E1 |
| [sc/](sc/) | Shader Core | E1 |
| [pa/](pa/) | Primitive Assembly | E1 |
| [tb/](tb/) | Tile Binner | E2 |
| [rs/](rs/) | Rasterizer | E2 |
| [tmu/](tmu/) | Texture Unit | E2 |
| [pfo/](pfo/) | Per-Fragment Ops | E2 |
| [tbf/](tbf/) | Tile Buffer | E2 |
| [rsv/](rsv/) | Resolve Unit | E2 |
| [mmu/](mmu/) | MMU | E1 |
| [l2/](l2/) | L2 Cache | E1 |
| [mc/](mc/) | Memory Controller | E1 |
| [csr/](csr/) | CSR Block (APB slave) | E3 |
| [pmu/](pmu/) | Perf Monitor Unit | E3 |

## 每個 block 子目錄結構(建議)
```
<block>/
  README.md              role, interface, status
  include/               public headers
  src/                   implementation
  tb/                    block-level UVM-SystemC testbench
```

## 命名
- module:`gpu::<block>::<ModuleName>`(C++ namespace + class UpperCamelCase)
- port:`*_i` / `*_o` suffix
