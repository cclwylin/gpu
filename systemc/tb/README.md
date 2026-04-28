# systemc/tb/ — System-Level Testbench

## Scope
- UVM-SystemC env for chip-level integration test
- Scene runner:load scene descriptor → push command stream → compare framebuffer vs `sw_ref`
- Scoreboard:per-stage boundary trace comparison

## Structure
```
tb/
  env/         UVM agents (APB master, AXI monitor, CSR driver)
  scenes/      scene runner + loader (link to tests/scenes/)
  scoreboard/  stage trace diff, framebuffer diff (pixel / SSIM)
  top_test.cpp top-level test driver
```
