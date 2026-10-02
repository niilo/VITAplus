# 24: Read and set the GPU power constraint

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 00

## Question

What power constraint does the GPU run under, and does `PWR_MAX` raise the frame
rate?

## Why it matters

The GPU is capped at about 1.0 GHz on this part. The same vendor GPU appears in
handhelds that ship with different power limits. If the driver is holding the
GPU below its maximum for thermal reasons, every "the GPU is too slow" result in
this plan is partly a power result, not a code result.

WinNative ships a Mesa build for Adreno that sets `KGSL_PROP_PWR_CONSTRAINT` to
`PWR_MAX` at queue creation and re-asserts it every 1000 submissions, so the
constraint does not slip back. That build is aimed at desktop Windows, where
the power profile is a driver setting. On Android the same property exists.

## Steps

1. Read the current state. Log the value of
   `/sys/class/kgsl/kgsl-3d0/max_gpuclk` and the mean of `gpuclk_khz` under
   load, from `tools/android/device.sh clocks`. If the mean is below the
   maximum while the title is GPU-bound, the constraint is real.
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
