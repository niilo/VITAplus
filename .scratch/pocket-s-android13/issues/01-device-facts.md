# 01: Record what the device actually reports

Status: open
Type: research
Label: ready-for-human
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

## Comments
