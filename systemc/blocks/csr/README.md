# CSR — Register Block(APB slave)

**Role**:APB slave,提供 register read / write,扇出 config 到各 block。

## Source of truth
- [`specs/registers.yaml`](../../../specs/registers.yaml)
- 本 block 由 `tools/regmap_gen/` 自動生成大部分 RTL + SystemC。

## Interface
- APB slave(32-bit data,address space 見 [`arch_spec.md §6`](../../../docs/arch_spec.md))
- Per-bank config output(同步到各 block 的 `*_cfg` port)

## Generated vs hand-written
- **Generated**:address decode、register storage、default values、reset values
- **Hand-written**:跨 bank 依賴、side-effect writes(e.g. `CP_RING_TAIL` 觸發 CP)

## Owner
E3
