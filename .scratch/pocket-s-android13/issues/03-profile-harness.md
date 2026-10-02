# 03: Build the profiling harness

Status: open
Type: task
Label: ready-for-agent
Blocked by: 01

## Goal## The harness carries the energy axis, not only the frame data

Everything here existed to answer "is it faster". The plan now asks "does it
cost less", so the harness has to produce power beside frames, or the protocol
cannot be followed. `tools/android/device_power_sample.sh` and
`tools/android/run_is_valid.sh` were written for that and are the reason a
rejected energy run never became a number.

Every measurement in this plan therefore runs three commands, not one:

```
tools/android/gameplay_scene.sh <pkg> <label>      # frames, presents, scenes
tools/android/device_power_sample.sh <csv> <secs>   # power, clocks, thermal
tools/android/run_is_valid.sh <dir> <csv>           # may this be quoted
```

## Goal

Every ticket after this one needs CPU and GPU numbers, not just FPS. FPS alone
cannot tell whether a change moved GPU time or CPU time. Build the tooling once.

## Steps

1. **simpleperf on a release build.** Add
   `<profileable android:shell="true" />` inside `<application>` in
   `android/app/src/main/AndroidManifest.xml`. Without it, `adb shell
   simpleperf record --app org.vita3k.emulator` fails on a release build. The
   tag only enables the shell uid to run the on-device `simpleperf` inside the
   app's own uid. It is safe in a production build.
2. **Symbols.** Document where the unstripped library is:
   `android/app/build/intermediates/cxx/<build>/<hash>/obj/arm64-v8a/libVita3K.so`.
   Add a helper to `tools/android/device.sh` that finds it, pushes it to
   `/data/local/tmp/native_libs/`, records with `--symfs`, pulls `perf.data`,
   and builds the binary cache with `binary_cache_builder.py` from the NDK.
3. **Thread names.** Threads created with `SDL_CreateThread`
   (`vita3k/kernel/src/kernel.cpp:192`) show up in a trace under `comm`, which
   is 15 bytes. Give the renderer thread (`vita3k/renderer/src/batch.cpp:328`),
   the GPU wait thread
   (`vita3k/renderer/src/vulkan/creation.cpp:61`) and the vblank thread
   (`vita3k/display/src/display.cpp:420`) short unique names, so a report can
   separate them. This is a small change and it makes every later profile
   readable.
4. **Perfetto.** Add a `perfetto` command to `tools/android/device.sh` that
   runs:
   `adb shell perfetto --txt -t <n>s -a org.vita3k.emulator -o /data/misc/perfetto-traces/<label>.pftrace ATRACE_CAT=sched ATRACE_CAT=freq ATRACE_CAT=gfx ATRACE_CAT=view ATRACE_CAT=hal ATRACE_CAT=sync ATRACE_CAT=idle ATRACE_CAT=power ATRACE_CAT=thermal ATRACE_CAT=membus sched/sched_switch sched/sched_blocked_reason power/cpu_frequency power/gpu_frequency gpu_mem/gpu_mem_total thermal/thermal_temperature`
   then pulls it. There is no `vulkan` or `egl` atrace category on Android 13.
   Do not add one.
5. **Surface timing.** Add a `latency` command that runs
   `adb shell dumpsys SurfaceFlinger --latency <layer>` in a loop and writes the
   three columns to a CSV. The layer name must match `SurfaceFlinger --list`.
   Record the layer name in the app log at start up so this works without
   guessing.
6. **Clocks and thermal.** `tools/android/device.sh` already has a `thermal`
   command at `device.sh:212-219` that samples
   `/sys/class/kgsl/kgsl-3d0/gpuclk` and
   `/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq`. Extend it rather
   than adding a second command, because one of those two sets of paths is
   wrong and only one command should exist. Add:
   - `gpuclk` (in Hz), `gpu_busy_percentage`, `max_gpuclk` and `throttling`
     from `/sys/class/kgsl/kgsl-3d0/`. Ticket 01 found that `gpuclk_khz` and
     `busclk_khz` do not exist on this device;
   - `cpu*/cpufreq/scaling_cur_freq` and `cpu*/cpufreq/cpuinfo_max_freq` per
     core. Ticket 01 found there is no `/sys/devices/system/cpu/policy*`, that
     `cpuinfo_cur_freq` is permission denied and that `scaling_cur_freq` reads
     fine;
   - `busclk_khz`, `max_gpuclk` and `throttling`;
   - a `temp_c` column. Ticket 01 found the zones that read are named
     `cpu-0-*` for cpu0 to cpu2, `cpu-1-*` for cpu3 to cpu7 and above, and
     `gpuss-*` for the GPU. Take the maximum of `cpu-0-*` and `cpu-1-*`. The
     zones named `pa`, `sdr*`, `mmw*`, `epm*` and `pmr735d*` return `Invalid
     argument` and must be skipped. The measurement protocol in `../spec.md`
     step 6 needs this column, and no other command produces it.
   Sample once a second into a CSV.
7. **Attribution.** Add `docs/adr/0001-android-profiling-tools.md`, which says
   how to answer
   three questions with these tools:
   - Which host thread burns the cycles: `simpleperf record --app <pkg> -g`
     then `report.py --sort comm,pid,tid`.
   - Where the renderer thread was not running: `simpleperf record -e
     task-clock:u --trace-offcpu`.
   - Whether the GPU or the CPU is the limit: compare `gpu_busy_percentage`
     with `task-clock` per frame from ticket 02.
   Do not use `dumpsys gfxinfo`. HWUI never sees a native Vulkan frame, so it
   reports nothing for this app.

## Acceptance

- Both targets build and the format check passes.
- `tools/android/device.sh perf <label>` produces a `perf.data` and an HTML
  report with symbols resolved.
- `tools/android/device.sh trace <label> 30` produces a pullable trace with
  `sched_switch` and `power/gpu_frequency` events.
- `tools/android/device.sh clocks <label> 60` produces a CSV with a GPU
  frequency column that moves while a game runs, and a `temp_c` column that
  carries a temperature or is empty with a note saying why.
- `docs/adr/0001-android-profiling-tools.md` exists and answers the three
  attribution questions below.

## Answer

## Comments
