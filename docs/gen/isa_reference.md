<!-- AUTO-GENERATED — do not edit. Regenerate via tools/isa_gen/. -->

# ISA Reference (v0.1)

## Parameters

- `instruction_width` = 32
- `warp_size` = 16
- `lane_count` = 4
- `gpr_count` = 32
- `const_count` = 16
- `varying_count` = 8
- `output_count` = 4
- `loop_stack_depth` = 8
- `tex_binding_count` = 16

## Floating point

- `format`: binary32
- `subnormal`: flush_to_zero
- `rounding`: round_to_nearest_even
- `nan_propagate`: True
- `transcendental_ulp_tolerance`: 3

## Opcodes

### Format: `alu`

| Name | Opcode | Ops | Semantics |
|---|---|---|---|
| `nop` | `0x00` (0b000000) | 2 | no op |
| `mov` | `0x01` (0b000001) | 2 | dst = src0 |
| `add` | `0x02` (0b000010) | 2 | dst = src0 + src1 |
| `mul` | `0x03` (0b000011) | 2 | dst = src0 * src1 |
| `mad` | `0x04` (0b000100) | 3 | dst = src0*src1 + src2 |
| `dp3` | `0x05` (0b000101) | 2 | dst.xyzw = dot3(src0, src1) |
| `dp4` | `0x06` (0b000110) | 2 | dst.xyzw = dot4(src0, src1) |
| `rcp` | `0x07` (0b000111) | 2 | dst.xyzw = 1/src0.x |
| `rsq` | `0x08` (0b001000) | 2 | dst.xyzw = 1/sqrt(src0.x) |
| `exp` | `0x09` (0b001001) | 2 | dst.xyzw = 2^src0.x |
| `log` | `0x0A` (0b001010) | 2 | dst.xyzw = log2(src0.x) |
| `sin` | `0x0B` (0b001011) | 2 | dst.xyzw = sin(src0.x) |
| `cos` | `0x0C` (0b001100) | 2 | dst.xyzw = cos(src0.x) |
| `min` | `0x0D` (0b001101) | 2 | dst = min(src0, src1) |
| `max` | `0x0E` (0b001110) | 2 | dst = max(src0, src1) |
| `abs` | `0x0F` (0b001111) | 2 | dst = abs(src0) |
| `frc` | `0x10` (0b010000) | 2 | dst = src0 - floor(src0) |
| `flr` | `0x11` (0b010001) | 2 | dst = floor(src0) |
| `cmp` | `0x12` (0b010010) | 3 | dst = src0>=0 ? src1 : src2 |

### Format: `flow`

| Name | Opcode | Ops | Semantics |
|---|---|---|---|
| `bra` | `0x20` (0b100000) | 2 | — |
| `call` | `0x21` (0b100001) | 2 | — |
| `ret` | `0x22` (0b100010) | 2 | — |
| `loop` | `0x23` (0b100011) | 2 | — |
| `endloop` | `0x24` (0b100100) | 2 | — |
| `break` | `0x25` (0b100101) | 2 | — |
| `kil` | `0x26` (0b100110) | 2 | discard pixel (FS only) |

### Format: `mem`

| Name | Opcode | Ops | Semantics |
|---|---|---|---|
| `ld` | `0x30` (0b110000) | 2 | dst = mem[src+imm] |
| `st` | `0x31` (0b110001) | 2 | mem[src+imm] = dst |
| `tex` | `0x34` (0b110100) | 2 | — |
| `texb` | `0x35` (0b110101) | 2 | — |
| `texl` | `0x36` (0b110110) | 2 | — |
| `texg` | `0x37` (0b110111) | 2 | — |

