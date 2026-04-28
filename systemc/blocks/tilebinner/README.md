# TB — Tile Binner

**Role**:Triangle → per-tile primitive list。

## Interface
- In:triangle from PA
- Out:DRAM writes(per-tile primitive list)
- Memory:MMU client

## Internal
- Bounding box compute
- Tile range iterator(tile = 16 × 16)
- Per-tile list writer(DRAM addr determined by bin manager)

## Output
- Per-tile list in DRAM,format:primitive descriptor + state pointer
- List size policy:fixed budget per tile,溢出時 chain 到下一塊

## Owner
E2
