# MMU — Memory Management Unit

**Role**:Virtual → physical translation,driver-managed page table。

## Interface
- Client:VF / TMU / L2(fb writes)/ CP(ring buffer fetch)
- Out:physical addr to L2 / MC
- CSR:`MMU_CTRL / MMU_PT_BASE / MMU_FAULT_*`

## Internal
- L1 TLB:fully-associative,16 entry
- L2 TLB:4-way set-associative,256 entry
- Page table walker(2-level,4 KB page)
- Fault capture(first fault latched)

## Policy
- TLB flush via CSR write
- Multiple outstanding walk OK(dep on walker 實作)
- Fault behavior:latch + IRQ,blocking 該 client

## Owner
E1
