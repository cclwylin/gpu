# CP — Command Processor

**Role**:Parse command ring buffer,maintain HW state,dispatch downstream。

## Interface(Phase 0 定)
- In:APB slave(CSR),AXI master(ring fetch,via MMU)
- Out:internal fanout — VF(draw),各 state block(state update),PMU(event)

## Internal
- FSM:IDLE / FETCH / DECODE / DISPATCH / WAIT
- Command decoder(見 [docs/microarch/commandprocessor.md](../../../docs/microarch/commandprocessor.md),Phase 0 產出)
- State scoreboard(追蹤 in-flight draw 依賴)

## Timing
- Target 1 packet / 4 cycle(avg)

## Owner
E1
