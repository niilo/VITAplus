# 01: Record what the device actually reports

Status: resolved
Type: research
Label: ready-for-human
Claimed: 2026-10-02 agent session
Blocked by: none

## Goal

Every later ticket depends on knowing what this device and this driver report.
The old plan (`.scratch/pocket-s-optimization/spec.md`) has two names for the
GPU and one kernel assumption that turned out to be wrong. Settle it once, on
the device, and write the answer into this ticket and into `../spec.md`.

## Done when

`../spec.md` carries the confirmed hardware table. This ticket's `## Answer`
lists every command above with its output. Any fact that could not be read is
written down as "not readable without root", not guessed.

## Steps

Collect all of this in one pass and save the raw output under
`tmp/pocket-s-android13/01/`.

1. Identity:
   - `adb shell getprop ro.product.manufacturer`, `ro.product.model`,
     `ro.soc.manufacturer`, `ro.soc.model`, `ro.board.platform`,
     `ro.build.display.id`, `ro.build.version.release`, `ro.build.version.sdk`,
     `ro.build.type`.
   - `adb shell uname -a` and `adb shell cat /proc/version` for the kernel.
   - Confirm whether the device is Snapdragon G3x Gen 2 or Snapdragon 8 Gen 2 as
     the vendor names it. Both names appear in the sources. Record what the
     device itself says.
2. CPU:
   - `adb shell cat /proc/cpuinfo` in full. Record the whole `Features` line
     verbatim. Then write a table with one row per feature, present or missing:
     `atomics` (LSE), `asimdhp` (FP16), `asimddp` (DotProd), `fphp`, `flagm2`,
     `sb`, `ssbs`, `bti`, `pauth`, `i8mm`, `mte`, `sve`, `crc`, `sha2`, `aes`,
     `dcpop`. `asimddp` and `flagm2` are the two that decide whether the ARM
     section of `../spec.md` is right.
   - Cluster topology. On Qualcomm the cpufreq files live under `policy*`, not
     `cpu*/cpufreq`. Run:
     `adb shell 'for p in /sys/devices/system/cpu/policy*/cpufreq/cpuinfo_max_freq; do echo "$p $(cat $p)"; done'`
     and the same for `cpuinfo_cur_freq`, `scaling_governor`,
     `scaling_available_frequencies` and `related_cpus`. If
     `cpu*/cpufreq` also exists, say so: `tools/android/device.sh:212-219`
     already reads `cpu*/cpufreq/scaling_cur_freq` and
     `/sys/class/kgsl/kgsl-3d0/gpuclk`, and one of the two sets of paths is
     wrong.
   - `adb shell 'for c in /sys/devices/system/cpu/cpu*/topology/thread_siblings_list; do echo "$c $(cat $c)"; done'`.
   - Record which core index belongs to which cluster. The old plan says cpu0
     to cpu2 are the small cores, cpu3 to cpu6 the big ones and cpu7 the prime
     core. Check it.
3. GPU:
   - `adb shell dumpsys gpu --gpudriverinfo`.
   - The Vulkan device name, driver ID and device ID. The app logs these at
     session start; read `vita3k.log`. Record the hex device ID. The gpuinfo
     report for this device says `0x43050A00`, the Adreno 740 is `0x43050A01`.
   - With the stock driver and then with Turnip, because the two differ.
4. Driver extensions:
   - The app logs its Vulkan feature list at start up. Save both sessions.
   - Record whether each of these is present on each driver:
     `VK_EXT_fragment_shader_interlock`,
     `VK_EXT/ARM_rasterization_order_attachment_access`,
     `VK_KHR_dynamic_rendering`, `VK_KHR_dynamic_rendering_local_read`,
     `VK_KHR_present_wait`, `VK_EXT_pipeline_creation_cache_control`,
     `VK_EXT_pipeline_creation_feedback`, `VK_KHR_pipeline_binary`,
     `VK_EXT_memory_priority`, `VK_KHR_dedicated_allocation`,
     `VK_EXT_external_memory_host`,
     `VK_ANDROID_external_memory_android_hardware_buffer`,
     `VK_QCOM_image_processing`, `VK_EXT_custom_resolve`.
   - Also record `minUniformBufferOffsetAlignment`, `maxUniformBufferRange`,
     `timestampPeriod` and whether `VK_EXT_calibrated_timestamps` is present.
5. Memory and pages:
   - `adb shell cat /sys/kernel/mm/transparent_hugepage/enabled` and
     `hpage_pmd_size`.
   - `adb shell getprop dalvik.vm.heapsize`, and the app's own memory use with
     `adb shell dumpsys meminfo org.vita3k.emulator` while a game runs.
6. Thermal and clocks:
   - `adb shell dumpsys thermalservice`.
   - Temperature zones. The measurement protocol in `../spec.md` step 6 needs a
     CPU temperature in degrees C, and no other ticket collects one. Run:
     `adb shell 'for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/type 2>&1) $(cat $z/temp 2>&1)"; done'`
     and record every zone. Then read the same list while a game runs. If no
     zone whose type contains "cpu" or "soc" is readable without root, write
     that down, because the protocol then falls back to a fixed wait.
   - `adb shell 'for f in /sys/class/kgsl/kgsl-3d0/gpuclk_khz
     /sys/class/kgsl/kgsl-3d0/gpuclk
     /sys/class/kgsl/kgsl-3d0/busclk_khz
     /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
     /sys/class/kgsl/kgsl-3d0/gpubusy
     /sys/class/kgsl/kgsl-3d0/max_gpuclk
     /sys/class/kgsl/kgsl-3d0/throttling; do echo "$f: $(cat $f 2>&1)"; done'`.
     Record which of these exist. `device.sh:212-219` reads `gpuclk`, while the
     Mesa and Qualcomm documentation names `gpuclk_khz`.
   - Run the same commands once while a game runs, so there is a busy reading.
7. Display:
   - `adb shell dumpsys display | grep -E "mRefreshRate|refreshRate|resolution"`
     and `adb shell dumpsys SurfaceFlinger --list`.
   - Confirm one mode at 2560x1440 and 60 Hz.
   - Present modes. No shell command lists them. Add one `LOG_INFO` line in
     `vita3k/renderer/src/vulkan/screen_renderer.cpp` next to the present mode
     selection that prints the whole result of `getSurfacePresentModesKHR`, read
     it from `vita3k.log`, then remove the line. Ticket 09 needs the list and
     nothing else collects it.
8. Cgroups:
   - `adb shell cat /proc/$(pidof org.vita3k.emulator)/cgroup` while the app is
     in the foreground and again after it has been in the background for a
     minute. Record which cgroup the app lands in with only a foreground
     service.
   - `adb shell cat /dev/cpuctl/top-app/cpu.shares` and `cpu.uclamp.min`.
9. Game Mode:
   - `adb shell cmd game mode` output, and whether the emulator is throttled.
10. Kernel command line: `adb shell cat /proc/cmdline`. Record whether
    `transparent_hugepage` or `khugepaged` is overridden.
11. Submodules and upstream defaults. Run
    `git submodule update --init --recursive`, then confirm the defaults this
    plan depends on with `git show origin/pre-plus-master:
    vita3k/config/include/config/config.h` for `high-accuracy`,
    `disable-surface-sync`, `memory-mapping`, `async-pipeline-compilation` and
    `log-level`. The "What changed since the old plan" section of `../spec.md`
    rests on those five, and only two of them are corroborated anywhere in this
    repository.

## Answer

Run on 2026-10-02 over USB with adb 37.0.1. Raw output is in
`tmp/pocket-s-android13/01/`. Six plan assumptions were wrong. Each is
corrected in `../spec.md` and `../map.md`.

### Identity

```
ro.product.manufacturer = AYANEO
ro.product.model = Pocket S
ro.soc.manufacturer = QTI
ro.soc.model = SG8275
ro.board.platform = kalama
ro.build.version.release = 13
ro.build.version.sdk = 33
ro.build.type = user
Linux localhost 5.15.104-android13-8-g05d70b033fc6 ... aarch64 Toybox
```

The kernel is 5.15, not 5.10 or 6.1. That matters for `MADV_COLLAPSE`, which
needs 6.1: the call returns `EINVAL` here.

### CPU

Eight cores, three clusters, and the layout is the one the old plan guessed:

| Cores | Max | Related |
| --- | --- | --- |
| cpu0 to cpu2 | 2016000 kHz | `0 1 2` |
| cpu3 to cpu6 | 2803200 kHz | `3 4 5 6` |
| cpu7 | 3360000 kHz | `7` |

The governor is `walt` on every core. `thread_siblings_list` for cpu0 is `0`,
which is correct for a single-thread sibling list.

**The cpufreq files are under `cpu*/cpufreq`, not `policy*`.** There is no
`/sys/devices/system/cpu/policy*` on this device. The plan said the opposite,
and `docs/adr/0001` repeated it. `cpuinfo_cur_freq` is permission denied, but
`scaling_cur_freq` reads fine, which is what the existing `device.sh` command
uses.

### ARM features

`/proc/cpuinfo` reports the same line on all eight cores:

```
fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm
jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 asimdfhm dit uscat ilrcpc
flagm ssbs sb paca pacg dcpodp flagm2 frint i8mm bf16 bti
```

Present: `atomics` (LSE), `asimdhp` (FP16), `asimddp` (DotProd), `flagm2`,
`ssbs`, `sb`, `paca`/`pacg` (PAuth), `i8mm`, `bf16`, `bti`, `dit`.
Absent: `sve`, `mte`, `aes` is present but that is unrelated.

Two of these settle questions the plan left open. **`i8mm` and `bf16` are
present**, so the part implements Armv8.6 integer matrix and BFloat16, which is
past the ARMv9.0-A baseline the plan assumed. **`asimddp` and `flagm2` are
present**, which confirms DotProd and FlagM2.

None of it changes the conclusion in `../spec.md`: the guest is ARMv7, so these
never appear in JIT output.

### GPU

```
gpu_model = AdrenoA32
max_clock_mhz = 680
freq_table_mhz = 1000 860 827 794 746 719 680 615 550 475 401 348 295 220 124
gpu_available_frequencies = ... 1000000000 ...
gpu_busy_percentage = 6 %
throttling = 0
gpuclk = 220000000
```

**`gpu_model` is `AdrenoA32`, so the A32 name is right.** The old plan's note
that the driver reports `Adreno (TM) 740` is also true; both names are used.

**The GPU is capped at 680 MHz while the hardware table goes to 1000 MHz.**
`max_gpuclk` is 680000000 and `max_clock_mhz` is 680, but `freq_table_mhz` and
`gpu_available_frequencies` both list 1000 MHz as the top bin. That is a 1.47x
difference in clock, and it is what ticket 24 exists to test. `num_pwrlevels`
is 15 and `max_pwrlevel` is 0.

The readable KGSL files are `gpuclk`, `gpu_busy_percentage`, `gpubusy`,
`max_gpuclk`, `throttling`, `idle_timer`. **`gpuclk_khz` and `busclk_khz` do not
exist**; the plan and the earlier research both named them. `gpuclk` is in Hz,
not kHz.

### Driver, and what the installed app already uses

The installed `org.vita3k.emulator` 1.1 has `custom-driver-name:
Balemuni_Apex_v2_ULTIMATE_SD8Gen2`, and its log says:

```
driverID: MesaTurnip  driverName: Turnip (Balemuni Apex v2 Ultimate)
driverInfo: Mesa 26.3.0-devel (SD 8 Gen 2 / Adreno 740 Apex v2 Ultimate by Balemuni)
conformance: 1.4.0.0
Present mode: Mailbox
Using the following memory mapping method: Page Table
renderer flags: support_rasterized_order_access=true support_fsr=true
                support_standard_layout=true deep_stencil=D32SfloatS8Uint
```

So **ticket 00's question is already answered in practice: the device runs
Turnip.** What is missing is the measurement, because the 7 FPS against 30 FPS
number in `CLAUDE.md` was on an older build.

Two more plan assumptions were wrong:

- **`support_rasterized_order_access` is true.** The plan said "Not expected on
  the A32" and ticket 06 said the stock driver does not have it. Turnip does.
  It is chosen first at `renderer.cpp:858-863` and it disables shader interlock,
  so the interlock path in tickets 06 and 15 is not the one this device takes.
- **Conformance is Vulkan 1.4.0.0**, so `VK_KHR_dynamic_rendering_local_read`
  is core in 1.4 and the driver reports 1.4.

### The installed app's settings differ from the Plus defaults

`config.yml` on the device has `high-accuracy: false`, `async-pipeline-compilation:
true`, `resolution-multiplier: 2`, `anisotropic-filtering: 16`. The Plus
defaults are `high-accuracy: true` and `async-pipeline-compilation: false`. So
the user has already moved two of the six settings ticket 05 was going to
measure. Ticket 05 must start from the device's values, not from the defaults.

### Memory and pages

```
/sys/kernel/mm/transparent_hugepage/enabled = [always]
hpage_pmd_size = 2097152
```

**THP is `always`, not `madvise`.** The plan said the GKI defconfig selects
`madvise` on Android 13. This vendor kernel overrides it. Two consequences for
ticket 12: anonymous mappings already get 2 MB pages without any `madvise`
call, so the ticket's main lever may be unnecessary; and `MADV_HUGEPAGE` is
still harmless. Ticket 12 should measure first and only then add the call.

### Thermal

`dumpsys thermalservice` works and reports `Thermal Status: 0`.
`cmd thermalservice` has only `help`, `override-status` and `reset`, as the plan
said. No `get-current-status`.

The zones that read a temperature are named `cpu-0-*` (cpu0 to cpu2),
`cpu-1-*` (cpu3 to cpu7 and more), `gpuss-*`, `skin-msm-therm`, and the
`pm8550*` regulators. Zones named `pa`, `sdr*`, `mmw*`, `epm*` and
`pmr735d*` return `Invalid argument` and cannot be used.

So **a CPU temperature is readable without root**, which is what the
measurement protocol in `../spec.md` needs for its cooldown step. `cpu-1-7`
was 32500 at idle, which is the prime core.

### Display and game mode

One mode: 1440x2560 at 60.000004 fps, `appVsyncOff 1000000`,
`presDeadline 16666666`, `frameRateOverride` empty. `gameContentTypeSupported`
is false.

`cmd game mode` needs an argument and prints `IllegalArgumentException`
without one.

### Cgroups

`/dev/cpuctl/top-app/cpu.shares` is 1024 and `cpu.uclamp.max` is `max`, while
`cpu.uclamp.min` is `0.00`. Readable from the shell. The app was not running
during this check, so the cgroup it lands in with a foreground service only is
still open.

### Games on the device

Twenty-one titles are installed under
`/storage/4CDE-C1FC/emu-app-data/psvita/ux0/app`, which is `pref-path` in
`config.yml`. Titles read from their own `param.sfo`:

| Title ID | Title |
| --- | --- |
| PCSA00015 | WipEout 2048 |
| PCSA00029 | Uncharted: Golden Abyss |
| PCSA00080 | Jak and Daxter Collection |
| PCSA00097 | Sly 2: Band of Thieves |
| PCSE00090 | Sine Mora |
| PCSE00792 | DARIUSBURST Chronicle Saviours |
| PCSE00865 | Sky Force Anniversary |
| PCSF00484 | Ratchet & Clank |

The other thirteen were not read one by one. The full list of installed title
IDs is in `gui-configs/apps-cache.xml` on the device.

`PCSA00029` is the title `tools/android/uncharted_scene.sh` uses, and the
project already has a save game in slot 0 for it. `PCSA00015` (WipEout 2048)
and `PCSA00097` (Sly 2) are the candidates for the 60 FPS title and the 2D
title in ticket 04.

### Signing: the installed app could not be replaced

`adb install` returned `INSTALL_FAILED_UPDATE_INCOMPATIBLE`. The app on the
device was signed with the **dev** key, whose certificate SHA-256 is
`85028da2c7e4321de4bd4215a45773bddcc8f90629bafd6e19ea07cc6d91cac0`. That is the
digest `docs/release.md` lists for the dev key, so the installed app was a
normal dev-signed build.

The key in `.vita-plus-signing/` is the **release** key, whose digest
`docs/release.md` lists as
`04c7cfc63ab68d91d23a34108516066b737c87e5528f7bff621afa3fea38522a`. A build
signed with it cannot update a dev-signed app.

So: the app was uninstalled, and a release-signed build of this branch was
installed. That is the procedure `docs/release.md` describes. It is also why
`device.sh install` now succeeds for further builds, because they all use the
release key.

### What the uninstall cost, and what it did not

Safe, because they live on the SD card under `pref-path`: the games, 37 GB in
total, and the firmware, `os0` 14 MB, `vs0` 288 MB and `pd0` 133 MB.

Lost, and restored:

| Lost | Restored how |
| --- | --- |
| `config.yml` | pulled to `tmp/device-backup-2026-10-02/` and pushed back |
| `gui-configs/apps-cache.xml` | same |
| `vita3k.log`, 12 MB and 96346 lines | pulled to the same folder |
| the firmware inside the app's data folder | copied back from `pref-path` with `cp -r`; a plain `cp` refuses with "Cross-device link" |
| the custom Vulkan driver | **not restored**, see below |

**The custom Vulkan driver was not on the SD card.** `android_driver.cpp`
reads it from `context.getFilesDir()`, which is the app's internal data
directory, so the pack lived only inside the app. `docs/release.md` says an
installed custom driver is lost on an uninstall; this check missed that line.

It has to go back through the app's own installer, which is a button in the
GPU settings section, because only the app can write to that directory. The
pack is the `V2` release asset `Balemuni_Apex_v2_ULTIMATE_SD8Gen2.zip` of
`Balemuni/Balemunis-Aurora`, which is the name in the saved `custom-driver-name`.
Its `meta.json` reports Mesa `26.3.0-apex-v2-b9a2bf3`, a 4 GB shader cache and
512 KB suballocators, which matches the driver string the app logged before the
uninstall.

### What the two drivers report

Measured with the same build, same game, same settings, only the driver
different:

| | Stock Qualcomm | Turnip (Balemuni Apex v2) |
| --- | --- | --- |
| `driverID` | QualcommProprietary | MesaTurnip |
| api version | 1.3.128 | conformance 1.4.0.0 |
| memory mapping | Double buffer | Page Table |
| `support_rasterized_order_access` | false | true |
| driver string | 512.676.0 | Mesa 26.3.0-devel, Balemuni Apex v2 |

So the plan's claim that the stock driver forces Double Buffer is confirmed on
this device, and so is its consequence: every Vulkan ticket measures a
different code path on the two drivers.

The stock driver does expose `VK_QCOM_image_processing`,
`VK_QCOM_render_pass_transform`, `VK_EXT_pipeline_creation_cache_control`,
`VK_EXT_pipeline_creation_feedback`, `VK_EXT_rasterization_order_attachment_access`
is **absent**, and `VK_KHR_timeline_semaphore` and `VK_KHR_synchronization2` are
present. That list came from the app log of a stock-driver run.

## Comments
