# 0001: Android profiling tools

Date: 2026-10-02

## Status

Accepted

## Context

The emulator runs on Android with a Vulkan renderer, a separate renderer thread,
one host thread per guest thread, and a vblank thread. Every performance question
in `.scratch/pocket-s-android13/` needs to separate three things:

1. Which host thread burns the cycles.
2. Where a thread was not running, which for the renderer thread means time
   blocked on a Vulkan fence or a present.
3. Whether the GPU or the CPU is the limit for a given title.

Frames per second alone answers none of them. A frame rate can hold while the
GPU clock rises, which means the GPU is doing more work for the same frame, or
while the renderer thread loses a core, which means the frame rate holds and the
input latency grows.

The device is Android 13 on an arm64 kernel, the app is a release build with R8
on and the native library stripped, and the process runs without root.

## Decision

`tools/android/device.sh` gains four commands, and one note records how to read
their output.

- `perf`: `simpleperf` against the package, with the unstripped
  `libVita3K.so` attached as symbols. Requires
  `<profileable android:shell="true" />` in the manifest, without which the
  shell cannot attach to a release build. The tag only lets the shell run the
  on-device profiler inside the app's own uid; no root is involved.
- `trace`: Perfetto from adb, with the `sched`, `freq`, `gfx`, `view`, `hal`,
  `sync`, `idle`, `power`, `thermal` and `membus` atrace categories, plus the
  `power/gpu_frequency` and `gpu_mem/gpu_mem_total` ftrace events. There is no
  `vulkan` or `egl` atrace category on Android 13.
- `clocks`: GPU and CPU frequency, GPU busy percentage and a CPU temperature,
  sampled once a second into a CSV. It extends the existing `thermal` command
  rather than adding a second one, because the two disagree about whether the
  Qualcomm cpufreq files live under `cpu*/cpufreq` or `policy*/cpufreq` and only
  one set of paths is right.
- `latency`: `dumpsys SurfaceFlinger --latency`, which gives desired, actual and
  ready timestamps for the last 128 frames.

## Consequences

- `dumpsys gfxinfo` is not used for this app. HWUI never sees a native Vulkan
  frame, so it reports nothing about the emulated picture.
- GPU hardware counters need Android GPU Inspector, which is not in this
  repository. `gpu_busy_percentage` from KGSL sysfs is the substitute, and it
  is readable without root.
- Thread names matter: the report separates threads by `comm`, 15 bytes. The
  renderer thread, the GPU wait thread and the vblank thread get short unique
  names.
- The measurement protocol in `.scratch/pocket-s-android13/spec.md` needs a CPU
  temperature for its cooldown step, so the `clocks` command carries one. If no
  temperature zone is readable without root, the protocol falls back to a fixed
  wait and says so.
