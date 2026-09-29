# 10: Find the best memory mapping mode

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 07

## Question

Which `memory-mapping` mode gives the best speed without errors on this
device?

## Context

- Default: `double-buffer` (`config.h:161`). It copies small buffers on
  every use and traps writes to large ones with page protection
  (`vulkan/renderer.cpp:1789-1869`).
- `page-table` and `native-buffer` avoid most copies, but they turn off
  dynarmic fastmem (`app/src/app_init.cpp:565`,
  `cpu/src/dynarmic_cpu.cpp:339-344`), so every guest memory access goes
  through a table. This costs CPU time on CPU-bound titles.
- `external-host` is not available on this GPU (`../spec.md`).
- Plus commit `14521654` forces double-buffer on the stock Qualcomm driver,
  because page table crashes there. Test page table and native buffer on
  Turnip only.
- The allowed modes are in the Memory Mapping setting list (ticket 06).

## Steps

1. With Turnip, run the protocol on all 4 titles: A = `double-buffer`,
   B = `page-table`. Then A = `double-buffer`, B = `native-buffer`.
2. Play each B mode for 5 minutes per title. Look for crashes, missing
   geometry, flicker and wrong colors. Take a screenshot of each problem.
   The user judges unclear cases in session H3.
3. Use ticket 07 data to explain each result: less copy time, or more JIT
   time.

## Output

Under `## Answer`: a table of mode, title, average FPS, percent at target,
99th percentile ms, problems. Then one mode for the preset, or "keep
double-buffer" if no mode is faster on all titles without problems.

## Answer
