<!-- AUTO-GENERATED — do not edit. Regenerate via tools/regmap_gen/. -->

# Register Map (v0.1)

- Address base: `0x00000000`
- Address size: `0x00020000`
- Bus: `APB` / data width: `32`

## Bank `CP` — Command Processor state + ring buffer control

- Base: `0x00000000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00000000` | `CP_CTRL` | RW | `0x00000000` | `ENABLE`[0], `RESET`[1], `HALT`[2] |
| `0x00000004` | `CP_RING_BASE` | RW | `0x00000000` | `ADDR`[31:0] |
| `0x00000008` | `CP_RING_SIZE` | RW | `0x00000000` | `LOG2`[4:0] |
| `0x0000000C` | `CP_RING_HEAD` | RO | `0x00000000` | `OFFSET`[23:0] |
| `0x00000010` | `CP_RING_TAIL` | RW | `0x00000000` | `OFFSET`[23:0] |
| `0x00000014` | `CP_STATUS` | RO | `0x00000000` | `IDLE`[0], `STALL`[1], `ERR`[2] |
| `0x00000018` | `CP_IRQ_STATUS` | RW1C | `0x00000000` | `DONE`[0], `ERR`[1] |

## Bank `MMU` — MMU + TLB control

- Base: `0x00001000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00001000` | `MMU_CTRL` | RW | `0x00000000` | `ENABLE`[0], `TLB_FLUSH`[1] |
| `0x00001004` | `MMU_PT_BASE` | RW | `0x00000000` | `ADDR`[31:0] |
| `0x00001008` | `MMU_FAULT_ADDR` | RO | `0x00000000` | `VADDR`[31:0] |
| `0x0000100C` | `MMU_FAULT_STATUS` | RW1C | `0x00000000` | `VALID`[0], `TYPE`[3:1], `CLIENT`[7:4] |

## Bank `FBO` — Framebuffer + MSAA config

- Base: `0x00002000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00002000` | `FBO_CTRL` | RW | `0x00000000` | `MSAA_EN`[0], `A2C_EN`[1], `RESOLVE_MODE`[3:2] |
| `0x00002004` | `FBO_COLOR_BASE` | RW | `0x00000000` | `ADDR`[31:0] |
| `0x00002008` | `FBO_DEPTH_BASE` | RW | `0x00000000` | — |
| `0x0000200C` | `FBO_WIDTH` | RW | `0x00000000` | `W`[13:0] |
| `0x00002010` | `FBO_HEIGHT` | RW | `0x00000000` | `H`[13:0] |

## Bank `TBF` — Tile buffer configuration

- Base: `0x00003000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00003000` | `TBF_CTRL` | RW | `0x00000000` | `BANK_CFG`[2:0], `BIST_START`[8] |
| `0x00003004` | `TBF_STATUS` | RO | `0x00000000` | `BIST_DONE`[0], `BIST_FAIL`[1] |

## Bank `SHADER` — Shader core configuration

- Base: `0x00004000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00004000` | `SHADER_VS_BASE` | RW | `0x00000000` | `ADDR`[31:0] |
| `0x00004004` | `SHADER_FS_BASE` | RW | `0x00000000` | — |
| `0x00004008` | `SHADER_SCRATCH_BASE` | RW | `0x00000000` | — |
| `0x0000400C` | `SHADER_SCRATCH_SIZE` | RW | `0x00000000` | — |
| `0x00004010` | `SHADER_UBO_BASE` | RW | `0x00000000` | — |

## Bank `TEX` — Texture binding slot table (16 slots x 16 B)

- Base: `0x00005000`, Size: `0x00001000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00005000` | `TEX0_BASE` | RW | `0x00000000` | `ADDR`[31:0] |
| `0x00005004` | `TEX0_CFG` | RW | `0x00000000` | `FORMAT`[3:0], `WRAP_S`[5:4], `WRAP_T`[7:6], `FILTER_MIN`[10:8], `FILTER_MAG`[12:11], `MIP_ENABLE`[13] |
| `0x00005008` | `TEX0_DIM` | RW | `0x00000000` | `WIDTH`[13:0], `HEIGHT`[29:16] |
| `0x0000500C` | `TEX0_MIP` | RW | `0x00000000` | `LOG2_LEVELS`[3:0] |

## Bank `PMU` — Performance counters + trace

- Base: `0x00010000`, Size: `0x00010000`

| Offset | Name | Access | Reset | Fields |
|---|---|---|---|---|
| `0x00010000` | `PMU_CTRL` | RW | `0x00000000` | `ENABLE`[0], `RESET`[1] |

