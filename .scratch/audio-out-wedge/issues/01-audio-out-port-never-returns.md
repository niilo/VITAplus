# 01: sceAudioOutOutput never returns and the game stops booting

Status: open
Type: task
Label: ready-for-agent

## Context

Every title on the Ayaneo Pocket S now stops booting with the same shape. The
game starts, creates its NGS racks, and then every guest thread is waiting:

```
audio_out_thread (101)   wait  sceAudioOutOutput         (last import 0x02DB3F5F)
SndStreamThread (102,105) wait  sceKernelWaitLwCond      on 'SndStreamThreadCond'
PCSA00029 main (8)        wait  sceKernelWaitThreadEnd   returns 0x80028005 WAIT_TIMEOUT
avPlayer FileStreaming    dormant, "forced through after 2ms; holder prio 185"
```

The emulator's watchdog follows:

```
HANG WATCHDOG: no framebuffer flip for 601 vblanks
  (renderer idle 600, guest wake-idle 0, SetFrameBuf calls=648 accepted=648)
```

No voice is ever processed: a failing session has zero `[NGSRATE]` lines,
where the working run has thousands.

The host audio side is alive. From logcat of a failing launch: `Low latency
audio enabled`, AudioFlinger created a track, `AudioTrack: stop(36): called
with 15876 frames delivered`. So SDL is running and being consumed.

This is not a regression from the branch under test. The same NGS code ran
Uncharted to the level at 31.21 FPS over 169 s at 20:40 on 2026-10-14
(`tmp/gameplay/ngs-keyoff-turnip`, `run_is_valid.sh` VALID), and the next run
at 20:47 failed and every run since has failed. Full record of what was ruled
out is in `.scratch/audio-out-wedge/spec.md`.

## Plan

1. Read the out port and the export it belongs to: `sceAudioOutOutput` in
   `vita3k/modules/SceAudioOut/` and the port in `vita3k/audio/src/`. Find the
   exact wait and name the thing that is supposed to signal it. Everything
   else in the guest is waiting behind this one call.
2. Read the SDL adapter: `vita3k/audio/src/sdl_adapter.cpp` or wherever
   `SDLAudioAdapter` lives. Establish which thread runs the callback and which
   object it drains. A port the callback never sees is exactly the shape of
   this bug.
3. Check `reset_android_session_audio` in the Android frontend. It runs on
   every title after the in-process relaunch, and it clears the adapter and
   the out ports. Establish whether a guest thread already blocked inside
   `sceAudioOutOutput` at that moment can ever be woken afterwards, and
   whether its port is still in the list the new callback drains. The message
   in the log is `Resetting Android audio backend 'SDL' after SDL session
   shutdown`.
4. Decide what should happen to a guest thread that is inside
   `sceAudioOutOutput` when its port goes away. Waking it with an error is
   better than leaving it blocked for the life of the process, because that
   thread is what the game waits on.
5. Reproduce without a game if the code allows it. The wedge is reached after
   the in-process relaunch, which every title does, so `sceNgsSystemInit`
   followed by an audio open is enough to tell a fixed build from a broken one
   on the host.
6. Test on the device: launch any title, wait 90 s, read `vita3k.log`. A fixed
   build has `[NGSRATE]` lines and shows the title screen. Use
   `tools/android/gameplay_scene.sh org.vita3k.emulator.debug <label>` and
   read `step-2-menu.png`, not `scene.png`, because the stall happens long
   before the level.

## Not in this ticket

The click at the end of a note, which is
`.scratch/log-review-2026-09-29/issues/09-ngs-key-off-release.md` step 4. That
needs a title that keys voices off and a person listening. Fixing this does
not produce it.

## Comments

### 2026-10-14: found while finishing ticket 08 on the device

Uncharted made zero `sceNgsVoiceKeyOff` calls in chapter 1, so the key-off
path could not be shown on the device anyway. The run that would have shown it
stalled instead, and so did every run after it.

Three titles fail the same way, across a clean rebuild of HEAD and of the
working tree, with `hle-counters` on and off, at 3.7 GB and at 18 GB free on
the card, after a reboot, and after clearing the emulator's `textures`,
`cache` and `shaderlog` directories. The one exception is the 20:40 run.

The cache clear moved the stall slightly: that session created no NGS racks at
all and stopped right after `sceIoOpen` on `app0:/gamedata.bin` and a probe
for the missing `SlotParam_1.bin`, where earlier sessions reached NGS init
first. That difference is not explained yet and belongs in the answer if this
ticket is worked.
### 2026-10-15: a bisect named a commit, and the commit is innocent

An older build, `00514fb7`, 25 commits behind master, boots Uncharted to the
level on this device. Two runs, both reached the saved chapter, 23.4 and 25.5
FPS. Master stalls every time it is tried. So the degradation is real and it is
in that range.

`git bisect start master 00514fb7`, judged per commit by building the APK,
installing it, launching Uncharted, waiting 100 s and reading the emulator's own
log, named this as the first bad commit:

    a99ad6f6  Return success from NGS exports on a voice whose rack was released

Its parent `593351a9` came back good, so the boundary is adjacent. It is the
wrong answer, and here is the proof.

Re-testing `a99ad6f6` itself on this device: 3169 `[NGSRATE]` lines, zero hang
watchdogs. It boots. A commit cannot be the deterministic cause of a stall that
a build containing it did not hit, and this one did not, earlier today at 20:40
as well.

Reverting it on top of master does not help either, in three steps:

1. the five `if (!system) return SCE_NGS_OK` in KeyOff, Kill, Pause, Play and
   Resume: still stalls
2. plus `sceNgsPatchCreateRouting` and `sceNgsVoiceGetOutputPatch`: still stalls
3. plus the `if (!patch) return SCE_NGS_OK` in SetVolume, SetVolumes and
   SetVolumesMatrix, which is all nine returns the commit introduced: still
   stalls

What the numbers actually say, counted runs rather than one sample each:

| Build | Boots | Stalls |
| --- | --- | --- |
| `00514fb7` and older recorded runs | 4 | 0 |
| master and master plus the NGS work | 1 | 7 |

So the failure is a timing-dependent stall that the newer code makes much more
likely, not a switch one commit flipped. A bisect with one sample per commit
cannot tell that apart from a single cause: a commit that shifts timing produces
exactly the same kind of boundary.

Do not repeat the bisect with single samples. Either run two or three times per
step, or work the plan in this ticket, which is about the wait that never
returns rather than about which commit introduced it.
