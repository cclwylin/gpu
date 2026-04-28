# systemc/ — SystemC Model

Phase 1(TLM-LT)→ Phase 2(PV/AT cycle-accurate)→ RTL 前的 golden reference。

```
systemc/
  common/        shared types, protocols, utilities, auto-generated headers
  blocks/        per-block modules (see blocks/README.md)
  top/           full-chip integration
  tb/            UVM-SystemC testbench + scoreboard
```

## Phases

| Phase | Coding rule |
|---|---|
| Phase 1 (M2.5–M9) | TLM-2.0 LT,functional only,SC_THREAD OK,STL OK |
| Phase 2 (M9–M16) | PV/AT pin-level,synthesizable subset(見 [coding_style.md §2.3](../docs/coding_style.md)) |

## Build
(tooling TBD Phase 0 end — CMake + SystemC 2.3.3 + UVM-SystemC)

## Testbench
- UVM-SystemC block-level
- cocotb + Python system-level
- Scoreboard 比對 `sw_ref` trace

## Owner
- E1(CP/SC/PA/MMU)
- E2(RS/TMU/PFO/TBF/RSV)
