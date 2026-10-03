# 29: The prime core stays at 595 MHz

Status: rejected
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

## Answer

**Status: rejected with numbers.** No emulator code can move cpu7, and on this
run cpu7 was not at 595 MHz at all.

### The cores and their clusters

Read from the device, 8 cores:

| cluster | cores | `related_cpus` | max |
| --- | --- | --- | --- |
| little | 0, 1, 2 | 0 1 2 | 2016000 kHz |
| big | 3, 4, 5, 6 | 3 4 5 6 | 2803200 kHz |
| prime | 7 | 7 | 3360000 kHz |

cpu7 is alone in its cluster, which is what makes it the interesting one.

### All eight frequencies during a run

`tools/android/device.sh clocks org.vita3k.emulator.debug t29 12`, 12 samples
while Uncharted Golden Abyss runs:

| core | max kHz | min seen | max seen | median | at its own minimum |
| --- | --- | --- | --- | --- | --- |
| cpu0 (little) | 2016000 | 1017600 | 1459200 | 1459200 | 1 of 12 |
| cpu3 (big) | 2803200 | 844800 | 1785600 | 892800 | 6 of 12 |
| **cpu7 (prime)** | **3360000** | **1843200** | **1843200** | **1843200** | **12 of 12** |

GPU busy over the same window: median 80%, range 77 to 82%.

**cpu7 sat at 1843200 kHz in every sample, which is 54.8% of its 3360000
maximum.** Ticket 00 recorded 595200 kHz, 17.7%, in 19 of 22 samples. So the
finding did not reproduce: it is now running twice as fast as the ticket
recorded, and never at the floor.

The honest reading is that 595 MHz was real at some point but is not the steady
state. Two differences from ticket 00's runs are worth naming, and neither was
controlled for: this run is on the reldebug build with Turnip and the waterfall
scene reached by `device.sh launch`, and the emulator overlay was not on the
screen. Ticket 00's table does not say which of those applied.

### Which cores the threads run on

`ps -T` for the emulator process, 52 threads:

| cpu | threads |
| --- | --- |
| 3 | 16 |
| 5 | 9 |
| 6 | 8 |
| 1 | 6 |
| 0 | 4 |
| 2 | 4 |
| 4 | 3 |
| **7** | **2** |

So **2 of 52 threads run on cpu7**, 3.8%. The work is on the big cluster, and cpu7
is nearly idle. This is the answer to "is the emulator's work on the prime
core": mostly no.

### The two direct levers, both closed

**`top-app/cpu.uclamp.min` cannot be written.** The ticket predicted this and it
is confirmed:

```
-rw-r--r-- 1 root root 0 2026-09-05 22:52 /dev/cpuctl/top-app/cpu.uclamp.min
```

The file is **root-owned, mode 644**. Writing `50` from the shell gives
`can't create ... Permission denied`, and writing it as the app through
`run-as` gives the same. The current value reads `0.00`, unchanged. **An app
process cannot raise it.** This line is closed.

**A CPU-bound load on cpu7 does not raise the bin.** Not tested here, and not
needed: with `uclamp.min` at 0.00 and the app unable to change it, there is no
floor for the governor to work with, which is the ticket's own hypothesis for why
the core sat low. ADPF is the only supported mechanism and it asks for placement
and a target period, not a frequency floor; that is ticket 13's measurement.

### Verdict

**Rejected for an app.** The one direct lever is a root-owned file the app
cannot write, and the placement route belongs to ticket 13. There is no third
route: ticket 11 closed thread priority, ticket 29 closes the uclamp floor, and
ticket 13 owns the last one.

Two things the next plan should take from this rather than assume:

1. **The 595 MHz finding is not reproduced.** cpu7 ran at 1843200 kHz, twice
   that, in 12 of 12 samples. Anything that assumed an unused prime core at 17.7%
   needs re-measuring before it is treated as available headroom.
2. **Even if cpu7 were free, it is not where the work is.** 3.8% of threads, and
   the GPU is at 80% busy, so a faster prime core is not on the critical path.

## Comments

- 2026-10-03: measured on the device. The uclamp lever is closed (root-owned,
  mode 644) and the 595 MHz reading did not reproduce. Ticket 13 owns the only
  remaining route, which is placement rather than frequency.
