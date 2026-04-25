# MC — Memory Controller

**Role**:AXI4 master,request scheduling,QoS。

## Interface
- Upstream:L2 miss、TB list write、RSV write、CP ring fetch
- Downstream:2× AXI4 128-bit master(`axi_tex`、`axi_fb`)

## Port allocation
- `axi_tex`:texture read、uniform read、command fetch
- `axi_fb`:framebuffer write、tile list read/write、depth (若走 DRAM)

## QoS
- Write > read on `axi_fb`(避免 RSV stall)
- Texture read 低優先(有 L1/L2 buffering)

## Phase 1
- Connect to DRAMSim3

## Owner
E1
