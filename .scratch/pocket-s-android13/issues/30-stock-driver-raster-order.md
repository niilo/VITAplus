# 30: Can the stock driver use rasterization order attachment access?

Status: open
Type: research
Label: ready-for-human
Blocked by: none

## Question

The stock Qualcomm driver runs Uncharted at 5.76 FPS and Turnip at 29.96, on the
same scene with the same draws per scene. The cause is that the stock driver
has neither `rasterization_order_attachment_access` nor shader interlock, so
the renderer falls back to `direct_fragcolor`, which puts a pipeline barrier on
the colour attachment before every programmable-blending draw.

The stock driver *lists* `VK_EXT_rasterization_order_attachment_access` and
`VK_ARM_rasterization_order_attachment_access` among its extensions, but the
feature query returns false. **Is that a dead extension entry, or a feature
that can be turned on?**

That is the largest open question in the plan. If the answer is yes, the stock
driver gains a path worth 5.2 times and the plan gains a fallback for every user
who cannot or does not want to install Turnip.

## What the code does, and why both answers are consistent

`vita3k/renderer/src/vulkan/renderer.cpp` treats the extension name and the
feature separately:

- The extension name is pushed into `device_extensions` as soon as it is
  enumerated, which is why it appears in the log's extension list.
- The feature is queried separately, and
  `PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT` is unlinked
  when `rasterizationOrderColorAttachmentAccess` is false.

That is exactly the state a driver with a real but default-off feature is in,
and it is also what a driver advertises an extension for but does not implement
looks like. The two resolved tickets disagree on this point, because ticket 01
recorded "absent" from an extension list and ticket 00 recorded "present" from
another. **Resolve it from one log line and say which.**

## Steps

1. Settle the record. Take one stock-driver session and read both lines from
   `vita3k.log`: the full device extension list, and the
   `renderer flags:` line that carries `support_rasterized_order_access`. Write
   down both, and correct whichever of tickets 00 and 01 is wrong.
2. Look for a way to enable it. In order of likelihood:
   - a driver property or environment variable. The Qualcomm blob reads
     `debug.vulkan.*` and `ro.hardware.*`; the Balemuni pack reads `TU_*`.
     Enumerate what the stock driver exposes by comparing the extension list
     between a clean boot and one with a property set. The property is the
     interesting part, not the env var.
   - `adrenotools_set_turbo()`, which `turbo-mode` already calls
     (`renderer.cpp:2388`). It may set a driver mode that also raises feature
     defaults. Test it: set `turbo-mode: true`, launch, and read the
     `renderer flags:` line again.
   - the Ayaneo power modes. They may set the same driver state. Test all
     three, the way ticket 24 does.
   - a config file the driver reads. `docs/adr/0001` records the Turnip cache
     directory; look for the Qualcomm equivalent.
3. If any of them flips `support_rasterized_order_access` to true, that is the
   answer: the stock driver can use the fast path and the plan gains a
   fallback. Measure the gameplay frame rate with it on, with
   `tools/android/gameplay_scene.sh`, and compare against ticket 00's 5.76 FPS.
   Check the picture at three fixed moments, because the stock driver has never
   run this path here and a wrong picture is worse than a slow one.
4. If nothing flips it, the answer is no, and that is worth writing down too: it
   means the stock driver is a permanent 5.2 times slower on this GPU for this
   emulator, and `CLAUDE.md` should say so plainly rather than leaving it as a
   preference.
5. Either way, record what the log line says on each driver, so nobody repeats
   the confusion between the extension list and the feature.

## Note on scope

This is stock-driver compatibility work, not an optimisation of the measured
driver. The plan measures on Turnip, and ticket 00 settled that. Run this
because the answer decides what a user without Turnip gets, and because the
extension-versus-feature confusion has already cost one wrong conclusion in
this plan. Do not let it delay a ticket whose result reaches the records.

## Acceptance

- One line from `vita3k.log` per driver, stating whether the extension is
  listed and whether the feature is enabled.
- A verdict: the feature can be turned on, with the setting that does it, or
  it cannot.
- If it can: the gameplay frame rate and the picture check.
- If it cannot: that statement in `CLAUDE.md`, so the stock driver is
  documented as the slow path.

## Answer

## Comments