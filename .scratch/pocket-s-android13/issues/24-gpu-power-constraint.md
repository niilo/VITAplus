# 24: Read and set the GPU power constraint

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00

## Question

The GPU is capped at 680 MHz while its own table lists 1000 MHz. What sets that
cap, and does raising it raise the frame rate?

## What is already known, from ticket 00

- The GPU never exceeds 680 MHz on either driver, in any run.
- Turnip reaches 29.96 FPS in gameplay at **93% GPU busy**. So at 30 FPS the cap
  is not the limit, and this ticket has no headroom to recover there.
- `max_pwrlevel` reads `0`, the fastest of the 15 levels, and `freq_table_mhz`
  lists 1000 MHz as the top bin. So the fast bin is selectable and something
  above the driver is choosing not to use it.

That leaves exactly one case where this ticket matters: **a 60 FPS title, and
sustained load**, where a higher clock might be the difference. Measure that, not
the 30 FPS title, which already holds its target.

Three hypotheses. The second is now the most likely:

1. `KGSL_PROP_PWR_CONSTRAINT`, which WinNative's Windows build sets to
   `PWR_MAX` at queue creation and re-asserts every 1000 submissions.
2. **A userspace governor or a vendor power mode.** `max_pwrlevel` is 0 and the
   top bin is selectable, so the driver is not holding it back. This is what
   `turbo-mode` and `adrenotools_set_turbo` touch, and what the Ayaneo power
   modes change. Test all three power modes on the device, not only the one the
   protocol names.
3. Thermal. Ticket 01 measured status 0 and 39.8 degrees C cold, but the GPU
   reached 66.7 degrees C during the ticket 00 runs, so it is not excluded.

`/dev/kgsl/kgsl-3d0` pwrlevel control needs root, so the app cannot set it. The
vendor power mode is the only route an app or a user can reach.

## What ticket 01 measured

```
gpu_model = AdrenoA32
max_clock_mhz = 680
freq_table_mhz = 1000 860 827 794 746 719 680 615 550 475 401 348 295 220 124
gpu_available_frequencies = 1000000000 ... 124800000
max_gpuclk = 680000000
num_pwrlevels = 15
max_pwrlevel = 0
```

The top bin in the frequency table is 1000 MHz and `max_pwrlevel` is 0, which
is the fastest level. So the driver reports the fast bin as available and
still runs at 680 MHz. That points at the power and thermal policy rather than
at a driver clamp, which is what this ticket has to tell apart.

## Why it matters

The GPU is capped at 680 MHz on this part, against a hardware table that
lists 1000 MHz. The same vendor GPU appears in
handhelds that ship with different power limits. If the driver is holding the
GPU below its maximum for thermal reasons, every "the GPU is too slow" result in
this plan is partly a power result, not a code result.

WinNative ships a Mesa build for Adreno that sets `KGSL_PROP_PWR_CONSTRAINT` to
`PWR_MAX` at queue creation and re-asserts it every 1000 submissions, so the
constraint does not slip back. That build is aimed at desktop Windows, where
the power profile is a driver setting. On Android the same property exists.

## Steps

1. Read the current state. Log `/sys/class/kgsl/kgsl-3d0/max_gpuclk` and the
   mean of **`gpuclk`**, which is in Hz, under load, from
   `tools/android/device.sh clocks`. `gpuclk_khz` does not exist on this kernel;
   ticket 01 measured that and the map records it.
2. Log the `KGSL_PROP_PWR_CONSTRAINT` value that the driver is created with.
   `VK_EXT_global_priority` is already read and applied at
   `vita3k/renderer/src/vulkan/renderer.cpp:836`; the KGSL power constraint is a
   separate property from the same source. Read how the driver maps it.
3. Add a temporary setting `gpu-power-constraint` with three values: 0 leaves
   the driver default, 1 asks for `PWR_MAX`, 2 asks for the lowest constraint.
   Apply it at queue creation. If the driver resets the constraint while
   running, re-assert it from the frame loop every 1000 submissions, behind the
   same setting.
4. Run A/B/A at 0, 1 and 2 on the title that runs badly and on the 30 FPS
   title. Record FPS, the frame interval 99th percentile, GPU clock mean and
   maximum, GPU busy percentage, and the `throttling` file.
5. Read the maximum GPU clock over 20 minutes as well as over 60 seconds. A
   constraint that helps for one minute and disappears after ten is not a
   result for a play session.

## Acceptance

- The constraint the driver uses, in a number or in the vendor enum name.
- The A/B/A numbers at 0, 1 and 2, over 60 seconds and over 20 minutes.
- Either a value the preset can set, or `Status: rejected` with the numbers.

## Answer

## Comments
