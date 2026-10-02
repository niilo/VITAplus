# 29: The prime core stays at 595 MHz

Status: open
Type: research
Label: ready-for-agent
Blocked by: 04

## Question

The emulator runs 3 guest threads, a renderer thread, a GPU wait thread and a
vblank thread. The prime core sits at its lowest bin for most of the run. Why,
and can anything be done about it?

## What ticket 00 measured

`tools/android/device_clock_sample.sh`, one sample per second, while Uncharted
Golden Abyss runs at resolution 2:

| Run | cpu7 (prime, max 3360 MHz) | cpu3 (big, max 2803 MHz) | cpu0 (small, max 2016 MHz) |
| --- | --- | --- | --- |
| stock | 595200 kHz in 19 of 22 samples | up to 1843200 | 1459200 |
| turnip | 595200 kHz in 12 of 18 samples | up to 1843200 | 1459200 |

`cpu7` is the only core in its own cluster: `related_cpus` is `7`, against `3 4
5 6` for the big cluster and `0 1 2` for the small one. So the governor parks
the one core that nothing else can use.

595200 kHz is 17.7% of the core's maximum. That is a large amount of unused
capacity on the fastest core in the system.

## Why this matters

The other CPU findings in this plan are about the threads. This one is about
the core, and it is not the same problem:

- Ticket 11 found the app cannot raise a thread's priority, because lowering
  the nice value needs `CAP_SYS_NICE` or a nonzero `RLIMIT_NICE` and an app
  process has neither.
- Ticket 13 uses ADPF, which asks the system to place threads on a suitable
  core. It does not ask for a frequency.
- Nothing in the app asks for cpu7 at all. The threads are placed by the
  governor and by cpuset, and cpu7 is not where the work is.

So even if ADPF works, it places the renderer thread on the *right kind* of
core. Whether cpu7 then runs fast is a separate question this ticket asks.

## Steps

1. Record the baseline. Extend the clock sample to all eight cores and to the
   three clusters, and record `top-app/cpu.uclamp.min` at the same time.
   Ticket 01 measured `cpu.uclamp.min` as `0.00`, which is the likely cause: a
   floor of zero lets the governor drop any core to its lowest bin. Write down
   whether the emulator's threads land on cpu7 at all, using the per-thread
   `simpleperf --per-core` from ticket 03.
2. Check what raises it. Three candidates, in the order to try them:
   - **A CPU-bound load on cpu7.** If the renderer thread is moved to cpu7 by
     ADPF in ticket 13, does the governor then raise the bin? Test with
     `taskset` from adb shell, which needs no root for its own process:
     `adb shell taskset -c 7 am start ...` does not work across users, so
     instead run the game and read the clock while a deliberate CPU load runs on
     cpu7 from the shell.
   - **`top-app/cpu.uclamp.min`.** Ticket 01 read `0.00`. If raising it is
     possible from the app, it is the direct lever. It is likely not: the file
     is under `/dev/cpuctl`, owned by the `system` uid, and an app cannot write
     it. Confirm, and if it is not writable, say so and stop that line.
   - **Load on the big cluster.** The big cores reach 1843200 kHz, which is
     66% of their 2803 MHz maximum, so the governor does use them. If putting
     more work on them is what raises the cluster, that is a scheduling result
     rather than a core result.
3. Decide whether this is worth a ticket with code in it. If the answer is that
   nothing in the app can reach it, set `Status: rejected` with the numbers. A
   measurement that says the hardware is out of reach is a result.
4. If ADPF in ticket 13 does raise the bin, record it there, because that is
   where the change lives.

## Acceptance

- All eight core frequencies for a run, and which cores the emulator's threads
  run on.
- The `top-app/cpu.uclamp.min` value while the game runs.
- A verdict: rejected with numbers, or a pointer to the ticket that changes it.

## Comments