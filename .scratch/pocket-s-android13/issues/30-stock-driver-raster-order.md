# 30: Can the stock driver use rasterization order attachment access?

Status: resolved
Type: research
Label: ready-for-agent
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

**The premise is wrong. The stock driver does not list the extension at all, so
there is nothing to turn on.** The confusion between tickets 00 and 01 came from
reading a mixed log.

### The one line per driver

Both lines are quoted verbatim from `tmp/gameplay/stock-gameplay/vita3k.log`,
which holds 31 device-creation sessions across both drivers.

**Stock Qualcomm**, line 42, the `All available device extensions:` line:

```
All available device extensions: VK_KHR_incremental_present, VK_EXT_hdr_metadata,
VK_KHR_shared_presentable_image, VK_GOOGLE_display_timing, VK_EXT_subgroup_size_control, ...
```

113 extensions, and **neither `VK_EXT_rasterization_order_attachment_access` nor
`VK_ARM_rasterization_order_attachment_access` is among them.** Counted
mechanically over that one line: zero occurrences.

**Turnip**, line 237, the same line from the same file:

```
All available device extensions: VK_KHR_incremental_present, VK_EXT_hdr_metadata,
VK_GOOGLE_display_timing, VK_KHR_8bit_storage, VK_KHR_16bit_storage, ...
```

142 extensions, and **both names are present.**

And the `renderer flags:` line, paired with the driver in each session:

| driver | `driverID` | exts | raster order ext | `support_rasterized_order_access` |
| --- | --- | --- | --- | --- |
| stock Qualcomm | `QualcommProprietary` | 113 | **absent** | **false** |
| Turnip | `MesaTurnip` | 142 | **present** | **true** |

**175 sessions across all seven recorded logs, zero exceptions.** The flag is
true in every `MesaTurnip` session and false in every `QualcommProprietary` one.
There is no configuration in the record where a stock-driver session has the
extension, and none where a Turnip session lacks it.

### The ticket's code reading was right about the code and wrong about what follows

The ticket says the extension name "is pushed into `device_extensions` as soon
as it is enumerated, which is why it appears in the log's extension list", and
infers from that the stock driver advertises it. The enumeration is real,
`renderer.cpp:748`:

```cpp
for (const vk::ExtensionProperties &ext : physical_device.enumerateDeviceExtensionProperties()) {
    available_extension_count++;
    available_extensions += ... ext.extensionName.data();
    auto it = optional_extensions.find(ext.extensionName.data());
    if (it != optional_extensions.end()) {
        *it->second = true;
        device_extensions.push_back(it->first.data());
    }
}
```

But this loop only sees extensions **the driver reports**. A name in
`optional_extensions` is a name to look for, not a name to print. So the stock
driver's list genuinely does not contain it, and the conclusion in the ticket's
"What the code does" section, that this is "exactly the state a driver with a
real but default-off feature is in", does not follow. It is the state of a
driver that never advertised the feature.

The feature query at `renderer.cpp:859` is never reached on the stock driver,
because `support_rasterized_order_access` is already false when it is entered,
and line 857 sets it false again if the extension is missing. So there was never
a default-off feature to flip. **Step 2's four candidate switches were searching
for something that does not exist**, and steps 3 and 4 do not need running.

### Why ticket 01 and ticket 00 disagreed

`tmp/gameplay/stock-gameplay/vita3k.log` is named for the stock driver but
contains both. The Balemuni pack, `Balemuni_Apex_v2_ULTIMATE_SD8Gen2`, is
injected in some sessions and falls back to the system loader in others, and
when it injects, `driverID` becomes `MesaTurnip` and the extension count goes
from 113 to 142. Ticket 00 read the extension list from a session where the
pack had injected. Ticket 01 read one where it had not. Neither was wrong about
the line they read; they read different lines of the same file.

This is the same failure mode the spec already warns about at step 9, one level
up: **a measurement that does not record which driver produced it is not a
measurement.** Every driver-dependent claim in this plan should now be quoted
with its `driverID`, not just its extension list.

### The verdict

**It cannot be turned on.** The stock driver does not expose
`rasterization_order_attachment_access`, so it stays on `direct_fragcolor`, and
the 5.2 times gap between the two drivers on this GPU stands. `CLAUDE.md` now
says so with the evidence rather than as an unexplained preference.

This closes the plan's largest open question. It also removes it from the
critical path: ticket 30 was worth running because it could have given users
without Turnip a fallback, and there is not going to be one.

## Comments

- 2026-10-03: answered from logs already on disk, with no device work and no
  code change. Steps 1 and 5 are done; step 2 is answered by step 1, because
  the extension is absent rather than disabled, so steps 3 and 4 were not run.
- 2026-10-03: the two drivers' extension sets were compared directly, not just
  counted. Of the 142 Turnip extensions, **50 are absent from the stock driver's
  113**, and the stock driver has **21 that Turnip does not**. So the 29-count
  difference hides an exchange of 71 extensions, and the two rasterization order
  names are only two of the 50. Worth remembering when a future claim says a
  driver "is missing an extension": compare both directions first.
- 2026-10-03: the finding is a reminder that `optional_extensions` is a lookup
  table, not a source of truth about the driver. A name in it is a name to look
  for, never one to print.
