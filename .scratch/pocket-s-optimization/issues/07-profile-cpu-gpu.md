# 07: Profile where the time goes

Status: open
Type: research
Label: ready-for-agent
Blocked by: 06

## Question

For each benchmark title: is the frame limited by the CPU or by the GPU,
and which part takes the time? The tuning tickets 09 to 15 depend on this.

## Steps

1. Make the release APK profileable: add
   `<profileable android:shell="true" />` inside `<application>` in
   `android/app/src/main/AndroidManifest.xml`. Without it, simpleperf
   cannot attach to a release app on a user build.
2. Build a release APK that keeps symbols for `libVita3K.so` on the host
   (the unstripped file under `android/app/build/intermediates/`), so
   simpleperf can name functions.
3. For each title, in its scene, for 30 seconds:
   - `simpleperf record -g -p <pid> --duration 30`, then
     `simpleperf report --sort comm,dso,symbol` with the unstripped
     library.
   - A Perfetto trace with the `sched`, `freq` and `thread state`
     categories.
   - `device.sh thermal` for GPU busy percent and GPU clock.
4. Sort the time into these parts: guest JIT code, dynarmic compile,
   HLE modules, texture decode and upload, surface sync and buffer copies,
   render thread Vulkan calls, fence waits, audio, and idle.
5. Mark each title "CPU-bound" (a thread near 100% on a core while GPU busy
   is under 80%), "GPU-bound" (GPU busy over 90%), or "mixed".
6. Set the target (30 or 60) for each title for the success criteria.

## Output

Under `## Answer`: per title, the top 10 functions with percent, the busy
percent of the top 5 threads and the cores they ran on, GPU busy percent,
the class, and the target. Then a list, in order, of the tickets from 09
to 15 that the data says are worth doing. Mark the others
`Status: rejected` with the reason.

## Answer
