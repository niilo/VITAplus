# 03: Build the profiling harness

Status: claimed
Type: task
Label: ready-for-agent
Blocked by: 01
Claimed: 2026-10-03 cline session (pocket-s-03-profile-harness)

## The harness carries the energy axis, not only the frame data

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

The code part is done on branch `pocket-s-03-profile-harness`, in commit
`cff2382c`, merged into `master` as `5dfe103a`. Verified on the
device (00314BHD01004402, Android 13) for the three commands that need no game
running: `clocks` and `trace` produce output, `latency` finds the layer.
`perf` still needs a device run against a built APK; see "What is left".

### 1. Manifest flag

`<profileable android:shell="true" />` is in
`android/app/src/main/AndroidManifest.xml`, inside `<application>` and before the
first activity. Without it the shell cannot run the on-device profiler inside the
app's own uid, and `adb shell simpleperf record --app org.vita3k.emulator` fails
on a release build. The tag needs no root and no permission.

### 2. Symbols

`device.sh perf` does the whole loop:

- finds the unstripped `libVita3K.so` under
  `android/app/build/intermediates/cxx` (newest match, because Gradle puts it
  under a hash directory), or takes `VITA3K_UNSTRIPPED_LIB`;
- pushes it to `/data/local/tmp/native_libs/`;
- records with `--call-graph dwarf --symfs`, because a release build strips
  frame pointers, so the frame-pointer call graph has nothing to walk;
- pulls `perf.data` and the symbols into `tmp/perf/<label>/`;
- builds the binary cache and the HTML report with `binary_cache_builder.py` and
  `report-sample` from `ANDROID_NDK_HOME`.

The raw `perf.data` and the symbols are the result and are pulled whether or not
the NDK is present. Only the HTML view needs it.

### 3. Thread names

`util::set_thread_name` is in `vita3k/util/`, next to `set_thread_nice`, because
both are host thread control. It is called from inside each thread, which is the
only place a thread may name itself.

| Name | Thread |
|---|---|
| `vita3k-render` | `render_loop` (`renderer/src/batch.cpp`) |
| `vita3k-gpuwait` | `VKContext::wait_thread_function` (`renderer/src/vulkan/context.cpp`) |
| `vita3k-vblank` | `vblank_sync_thread` (`display/src/display.cpp`) |
| `vita3k-watchdog` | `freeze_watchdog_thread` (`display/src/display.cpp`) |

The watchdog was not in the ticket. It is started from inside the vblank thread
and shows up in every trace as an unnamed thread, so it is named here too.

`comm` is 16 bytes including the terminating zero. A name of 16 or more
characters is **refused** and logged, not truncated: prctl does not report a cut,
and a truncated name can collide with another thread in a report, which is the
one failure this function exists to prevent. All four names are 13 to 15
characters and fit.

On Windows the function returns false and does nothing. Nothing in this
repository measures Windows, so there is no `SetThreadDescription` call to
justify. macOS uses `pthread_setname_np`.

### 4. Perfetto

`device.sh trace <package> <label> <secs>` runs the `sched`, `freq`, `gfx`,
`view`, `hal`, `sync`, `idle`, `power`, `thermal` and `membus` atrace categories
plus the `sched/sched_switch`, `sched/sched_blocked_reason`,
`power/cpu_frequency`, `power/gpu_frequency`, `gpu_mem/gpu_mem_total` and
`thermal/thermal_temperature` ftrace events, and pulls the result to
`tmp/trace/<label>/trace.pftrace`.

Measured on the device: a 10 second recording of the running emulator produced a
3.5 MB trace, and `strings` on it finds `sched_switch`, `power/gpu_frequency`,
`gpu_mem_total`, `thermal_temperature` and `cpu_frequency`.

There is no `vulkan` or `egl` atrace category on Android 13, so none is asked
for.

### 5. Surface timing

`device.sh latency` reads the layer name from `dumpsys SurfaceFlinger --list`
rather than hard-coding it, because the name carries a hash that changes when the
activity is recreated. The bookkeeping entries SurfaceFlinger also lists for the
same package (`ActivityRecord`, `ActivityRecordInputSink`, `WindowToken`,
`StartingWindow`) are filtered out, so the app's own surface is selected. The
name is written to `tmp/latency/<label>/layer.txt` next to the CSV.

Measured on the device: the layer was found and `--latency` answered with a
refresh period of 16666666 ns, which is 60 Hz. The three per-frame columns were
all zero, because the device was locked and the app had presented nothing. So the
command is correct and the CSV is empty for that reason. **An unlocked device with
the game in play is needed to confirm the columns carry values.**

### 6. Clocks and thermal

The `thermal` command is now `clocks`, and it is the only one. The two disagreed
about whether the Qualcomm cpufreq files live under `cpu*/cpufreq` or
`policy*/cpufreq`, and only the first is right on this device, so one command
means one set of paths. The old name still runs and prints where the new one is,
so an old command line fails with a message instead of a silent no-op.

Columns, measured on the device in a 3 second run:

```
second,gpuclk_hz,gpu_busy_pct,max_gpuclk_hz,throttling,temp_gpu_mdeg,
cpu0_khz,cpu0_max_khz,cpu3_khz,cpu3_max_khz,cpu7_khz,cpu7_max_khz,
temp_cpu0_mdeg,temp_cpu1_mdeg
```

Every column carries a value, so the header and the rows agree. `temp_gpu_mdeg`
reads the KGSL `temp`. `temp_cpu0_mdeg` is the maximum over the `cpu-0-*` zones
and `temp_cpu1_mdeg` the maximum over the `cpu-1-*` zones, which keeps the two
clusters apart. An earlier version collapsed them into one number and gave two
columns the same value.

Sample from the device, with the emulator running and the GPU idle:

```
gpuclk: mean 680 MHz, min 680 MHz over 3 s
gpu busy: mean 1.0 percent
CPU temperature: peak 41.0 C (cpu-0 cluster), 42.0 C (cpu-1 cluster)
```

This confirms two facts the map already holds and adds one detail:

- `max_gpuclk` is 680 MHz and the observed clock is 680 MHz even when the GPU is
  idle, so the 1000 MHz in the hardware table is not reachable on this device.
  Ticket 24.
- `cpu7_max_khz` is 3360000 and `cpu7_khz` reads 595200, so the prime core runs
  at 18% of its maximum while the emulator is up. Ticket 29.

`device_clock_sample.sh` and `device_power_sample.sh` were left alone. The first
was written before the retarget and `clocks` covers it; ticket 29 references it,
so it is not deleted here.

### 7. ADR

`docs/adr/0001-android-profiling-tools.md` exists and is correct as written. Two
details it did not mention were learned while running the commands, and are
added there: the layer name has to be filtered out of the SurfaceFlinger
bookkeeping entries, and a `comm` name that does not fit is refused rather than
truncated.

### 8. Tests

Three googletests in `vita3k/mem/tests/thread_priority_tests.cpp`, next to the
existing four for the nice value: a name that was set reads back, 15 characters
are accepted and 16 are refused without changing the thread, and an empty name
and a null pointer are refused. The read-back tests skip off Linux, where
`PR_GET_NAME` does not exist.

### What is left

The release APK **was** built, in the main checkout, after this ticket merged:
`container/vita3k-docker.sh android release` gives
`build/android-apk/app-release.apk`, and the `profileable` tag with its `shell`
attribute is present in that APK's manifest. So the APK that `perf` was waiting
for now exists.

**This alone is not enough, and the next section explains why.** Building the
APK was necessary but not sufficient: the APK has to reach the device as well,
and the one that is installed there is signed with a different key and has no
`profileable` tag.

Still to do, both needing the game in play on an unlocked device:

- `device.sh perf <package> <label>` against that APK, to confirm `perf.data`
  and the HTML report come out with symbols resolved. The report step also needs
  `ANDROID_NDK_HOME` to point at an NDK; without it the command still pulls
  `perf.data` and the symbols, which are the raw result.
- `device.sh latency <package> <label>` on an unlocked device, to confirm the
  three frame columns carry values. On a locked device they read zero.

Both need the game reachable, so they are the human half of this ticket. The
ticket stays `claimed`.

### The device half was run on 2026-10-03, and both items have a blocker

The device (AYANEO Pocket S, Android 13, `00314BHD01004402`) was unlocked and on
AC power at 75 percent, thermal status 0. Uncharted: Golden Abyss (PCSA00029) was
walked into the waterfall chapter with Turnip 26.3.0 and the camera moved. The
run is in `tmp/latency-run/`.

**`device.sh latency` finds the layer and gets no frames out of it.** It picked
`991c8a0 org.vita3k.emulator/org.vita3k.emulator.Emulator#32708`, which is a real
layer, and wrote the correct header. After 60 seconds the CSV held the header and
nothing else. `dumpsys SurfaceFlinger --latency <layer>` returns only the refresh
period `16666666` and no frame rows, for that layer and for the
`SurfaceView[...](BLAST)` layer the app actually draws into. `--latency-clear`
followed by `--latency` changes nothing.

So this item cannot be closed as written: the interface `device.sh latency` uses
carries no frame data for this app on Android 13. The frame timing it was after
is already in `presents.csv` from `perf-log`, which records every host present
with a timestamp, and `perf_report.py` reports it. The SurfaceFlinger columns are
redundant with that, so the fix is to drop them rather than to find another
dumpsys flag.

**`device.sh perf` is blocked by the signing key, not by the build.** The APK
installed as `org.vita3k.emulator` does **not** have the `profileable` tag, so
`simpleperf record --app` cannot run against it. A new APK is needed, and it
cannot go over the installed one:

| APK | certificate SHA-256 | profileable |
| --- | --- | --- |
| installed `org.vita3k.emulator` | `04c7cfc6...38522a`, the release key | no |
| `build/android-apk/app-release.apk` | `c6520e3f...155b8` | yes |

`docs/release.md` lists the release digest as `04c7cfc6...` and the dev digest as
`85028da2...`, so the built APK carries a third certificate. The cause is
`android/app/build.gradle:78`: the `release` build type falls back to
`signingConfigs.debug` when `hasCiSigning` is false, so a plain
`container/vita3k-docker.sh android release` signs with the Gradle debug key
rather than with `.signing/dev/`.

`adb install -r` would fail with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`. The two
ways through both have a cost, and this is a decision for the user, not for an
agent:

1. Rebuild with `VITA_SIGN_WITH_RELEASE_KEY=1`. The digest then matches the
   installed app, `adb install -r` updates it in place, and app data survives, so
   the Turnip pack and the config are untouched. `container/build-android.sh`
   sources `.signing/release/signing.env` itself, so no password reaches a
   command line.
2. Uninstall and install the built APK. That destroys the Turnip driver pack,
   which lives in the app's own files directory and is not on the SD card, and
   the pack has to go back through the app's installer by hand. This is the
   procedure ticket 01 already paid for.

Option 1 is the one that keeps the device as it is. Nothing was uninstalled on
2026-10-03 and all 21 titles and the driver pack are still in place.

`ANDROID_NDK_HOME` is unset on this host, so `device.sh perf` would pull
`perf.data` and the symbols but build no HTML report. The NDK is inside the
Android container image, not on the host.

### The report tool was confirmed on this run

`perf_report.py` over `tmp/latency-run/scene/` with `--target 30 --warmup 120`:

| line | value |
| --- | --- |
| driver | Turnip-v26.3.0-20261002-r5 |
| average FPS | 29.86 |
| frame interval p99 | 47.85 ms |
| frame interval max | 78.36 ms |
| late frames (over 50.0 ms) | 18 of 2389 (0.75%) |
| presents per second | 29.88 |
| SurfaceFlinger interval p99 | no latency.csv |

No watchdog fired in that session and `scene.png` is the waterfall chapter, not a
menu. **This is not a baseline.** There is no power CSV, so `run_is_valid.sh`
has nothing to check criterion 6 against, and no A/B/A/B/A set was run. It is
quoted only because it exercises every column of the report on real files.

Note the p99 of 47.85 ms against the 50 ms limit of criterion 2. The plan's
retarget calls the earlier 43.43 ms reading a playability defect, and this run is
close to the same figure, so the steadiness question in `../spec.md` is still
open.

## Comments

- The submodules were shared with the main checkout and their working trees were
  empty in this worktree, so `container/vita3k-docker.sh build` first failed on
  "Submodule external/ffmpeg is empty" and then on "ARCHITECTURE variable is not
  set up", because `external/dynarmic/CMakeModules/DetectArchitecture.cmake` was
  missing too. Each was restored with
  `git --git-dir=<gitdir from the submodule .git file> --work-tree=<path> checkout -f <recorded sha>`.
  A worktree needs this after `git submodule update --init --recursive`: the
  update reports success and leaves the working trees empty, because the
  gitdir link in each submodule is relative and does not resolve from a
  worktree.
- `git diff` shows 21 `external/*` lines changed. That is the same artifact and
  must not be committed. Only `vita3k/`, `tools/`, `android/` and `.scratch/`
  belong in the commit.
- **The Android APK cannot be built from a git worktree, for a reason unrelated
  to this ticket.** SDL's own `external/sdl/cmake/GetGitRevisionDescription.cmake`
  walks up from its source directory looking for a `.git` **directory**, and then
  reads `HEAD` and `packed-refs` from it. A worktree has `.git` as a *file*, and
  the container mounts only the worktree at `/src`, so no `.git` directory exists
  there at all. The result is `CMake Error: File /src/.git/HEAD does not exist`,
  and after working around that, `File /src/.git/packed-refs does not exist`.

  This was confirmed to be pre-existing: `container/vita3k-docker.sh android
  release` was run with this ticket's manifest change stashed, so the tree was
  clean, and it failed with the same error 7 times in the log. It is not caused
  by the `<profileable>` tag.

  `processReleaseMainManifest` passes, which is the task that parses the
  manifest, so the manifest change itself is good. `:app:configureCMakeRelease`
  is the task that fails, and it fails in SDL.

  **So `device.sh perf` cannot be verified here.** It needs an APK, and the APK
  needs a normal checkout. On this machine `container/vita3k-docker.sh android
  release` has to be run in the main checkout at `/home/pielinen/src/VITAplus`,
  not in a worktree.
