# The audio out port wedges and every game stops with it

Found on 2026-10-14 while trying to finish
`.scratch/emucorev-fixes/issues/08-ngs-key-off-invokes-finish-callback.md`
step 7 on the Ayaneo Pocket S. It is not one of the two comparisons this
repository is built on, and it blocks any device measurement, so it has its
own directory.

## What happens

A title boots, creates its audio system, and then the whole guest waits. The
screen stays black, the emulator's vblank watchdog reports no framebuffer flip,
and no voice is ever processed. Three titles do it: Uncharted: Golden Abyss
(PCSA00029), Killzone Mercenary (PCSA00107) and Dariusburst Chronicle Saviours
(PCSE00792).

The thread dumps always have the same head. Every guest thread is `wait` or
`dormant`, and the chain starts at the audio out call:

```
audio_out_thread (101)   wait  sceAudioOutOutput         never returns
SndStreamThread (102,105) wait  sceKernelWaitLwCond      no buffers arrive
PCSA00029 main (8)        wait  sceKernelWaitThreadEnd   times out
SceGxmDisplayQueue (65)   dormant
```

The renderer is idle rather than busy: `SetFrameBuf calls=648 accepted=648`
and no further flips, while the guest scheduler keeps handing the CPU around.

## It is not the branch under test

The last run that worked was 2026-10-14 20:40 with the same NGS code that is
on the branch now: `tmp/gameplay/ngs-keyoff-turnip`, 31.21 FPS over 169 s,
`driverID: MesaTurnip`, `run_is_valid.sh` VALID. The first failure was the
next run at 20:47 the same evening, and nothing has booted since.

## Ruled out

| Candidate | How it was ruled out |
| --- | --- |
| The NGS branch | the same build ran the level at 20:40 |
| Six uncommitted renderer files | clean HEAD and the working tree both stall |
| `hle-counters` | on and off both stall |
| Disk space | stalls at 3.7 GB free and at 18 GB free |
| A wedged device | a reboot and a card re-seat do not clear it |
| Emulator caches | clearing `textures`, `cache` and `shaderlog` changes nothing |
| The Vulkan driver | Turnip in every failing run; the stock driver fails too |
| The game's save | Dariusburst, untouched since September, fails the same way |

## The host audio side works

From logcat during a failing launch:

```
SDL/APP : Low latency audio enabled
AudioFlinger: createTrack_l(): mismatch between requested flags ...
AudioTrack: stop(36): called with 15876 frames delivered
```

The SDL device opens and AudioFlinger is consuming frames, so whatever is
missing is between the SDL callback and the port the guest is waiting on.
There are only two audio settings, `audio-backend: SDL` and `audio-volume:
100`, both at their defaults.

## One structural thing worth reading first

Every run, working or not, relaunches the title in process and resets the
Android audio backend:

```
[SDL_main]: Relaunching in-process with self 'uncharted.self'
[reset_android_session_audio]: Resetting Android audio backend 'SDL' after SDL session shutdown
```

It appears in the working 20:40 run as well, so it does not separate the two.
It is still the place where a port can be left waiting on something that no
longer exists.

## Logs

- `tmp/gameplay/ngs-keyoff-turnip/` the last run that reached the level
- `tmp/gameplay/after-reboot/` a clean-HEAD failing run with step screenshots
- `tmp/kz-walk/` the Killzone walk and the cache-clear run
- `tmp/gameplay/space-turnip/` a failing run with 18 GB free on the card

## The measurement record

`issues/02-current-builds-fail-to-boot.md` holds the runs, the counted
boot-and-stall totals, the bisect and the three reasons its answer is wrong.
The short version: an older build boots every time, current builds stall 16
times out of 19, and no single commit is responsible, because the failure is a
timing-dependent stall rather than a switch one commit flipped. It is not a
frame rate regression; where the current build runs at all it is the faster of
the two.