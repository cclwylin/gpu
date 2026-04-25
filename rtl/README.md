# rtl/ — Verilog RTL(Phase 3)

## Phase
Phase 3 才開始(M16–M23.5)。Phase 0–2 期間保持空殼。

## 決策
- 手寫 Verilog(方案 A,非 HLS)
- SystemC PV model(from systemc/)當 golden reference
- Block-by-block UVM equivalence check

## 結構
```
rtl/
  blocks/<abbr>/    per-block RTL (module + block-level tb)
  top/              chip-level integration
  tb/               system-level testbench
```

## Coding rule
- 見 [docs/coding_style.md §4](../docs/coding_style.md)
- Single clock domain,async-assert sync-deassert reset
- Scan-ready(no latch,no gated clock in RTL,no initial in DUT)

## Owner
- E1(SC、MMU)
- E2(RS、TMU、PFO、RSV、CP、fixed-function rest)
