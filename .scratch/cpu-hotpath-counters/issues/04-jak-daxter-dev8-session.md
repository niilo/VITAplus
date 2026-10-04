# 04: What the first playable Jak and Daxter session showed

Status: resolved
Type: research
Label: ready-for-agent

## What ran

Build `v1.2.1-dev.8`, commit `3467d9fd`, branch
`fix/ngs-voice-released-rack-exports`. Debug package
`org.vita3k.emulator.debug` on the Pocket S, Android 13. Driver
`MesaTurnip`, Mesa 26.3.0-devel, conformance 1.4.6.1.

The log is `tmp/play-jak/vita3k.log`, 507 MB, 3679010 lines. The session
starts at line 103543 and runs to the end of the file.

**Zero crashes in that session.** `fatal signal` appears 13 times in the
file, all before line 103543. The four crash fixes hold: several minutes of
continuous play with no fault.

## The frame rate is not GPU bound on this title

Measured live while the app was running, PID 20915:

| | |
| --- | --- |
| GPU busy | 13, 13, 43, 39, 40 % |
| GPU clock | 680 MHz |
| app CPU | 79 % of one core, then 207 and 211 % across cores |
| swapchain | 2560x1440 |
| resolution multiplier | 2 |
| present mode | Fifo, v-sync on, 4 images |

Ticket 00 recorded the GPU at 93 to 99 % busy and concluded the GPU is the
limit. **On Jak and Daxter that is not true.** The GPU is between 13 and 43 %
while the CPU is the busiest resource, so the CPU-side tickets are the ones
that matter for this title, not the GPU ones. Any claim about the GPU being
the limit has to name the title it was measured on.

## The prime core is pinned at 595 MHz

`/sys/devices/system/cpu/cpu7/cpufreq/scaling_cur_freq` read 595200, and the
other cores read 844800 to 1843200. Ticket 29 recorded exactly this 595 MHz
figure and was rejected on the grounds that it was the expected low bin for
the prime core. It is worth re-testing with the device under load, because a
prime core at the lowest bin while the CPU is the limit would explain a large
part of the frame rate on its own.

## Two settings are candidates for a quick frame rate gain

Neither is measured, both are one config value:

1. **Resolution multiplier 2.** The swapchain is 2560x1440. At multiplier 1
   it would be 1280x720, a quarter of the pixels. Ticket 20 owns this.
2. **40 rejected shaders.** `SPIR-V parsing FAILED` with
   `Source (%1536) and destination (%1537) of OpBitcast must have the same
   total number of bits`, at 9 distinct offsets, spread from 00:28:11 to
   00:33:29, so they are hit repeatedly and not only at load. A driver-side
   rejection of our generated SPIR-V is a translator bug, not a driver one,
   and it may fall back to a slower path.

## The audio path initialises and then produces nothing

The audio chain is built and then never runs:

| line | what |
| --- | --- |
| 00:27:12 | thread `audio_out_thread` created |
| 00:27:14 | `AacDecoderState resampler created at 48000 Hz` |
| 00:27:58 | `[NGSRATE] stereo rate resampler created, #1 (44061 -> 48000 Hz)`, once for the whole session |

Both audio threads, `audio_out_thread` (77) and `AudioOutput` (110), are
blocked in `sceAudioOutOutput`, NID `0x02DB3F5F`, in 60 hang dumps between
00:27:24 and 00:27:44. `sceAudioOutOutput` is implemented at
`vita3k/modules/SceAudio/SceAudio.cpp:191` and blocks in
`AudioState::audio_output` at `vita3k/audio/src/audio.cpp:101`, whose wait is
a sleep in `std::this_thread::sleep_for` at `audio.cpp:120`. That is pacing,
not a deadlock. The hang dumps stop at 00:27:44 while the session continues to
00:34:40, so they are an early-startup artefact and not evidence of a later
hang.

**Do not read "sceNgsSystemUpdate is called twice" from this session.** That
count was wrong: both matches are `STALE HOST POINTER` lines that mention the
import name in passing. The second session section replaces this with what the
longer run shows, which is that the channel count is what the game asserts on.

## The game asserts about channel count, and it is the game's own

`Assertion failed: patchRouteInfo.nOutputChannels == 2` appears from
00:28:10.900, in a cluster of 8 `sceNgs*` error returns inside 21 ms:
`sceNgsPatchCreateRouting`, `sceNgsVoiceKill`, `sceNgsPatchGetInfo`,
`sceNgsVoicePatchSetVolumesMatrix`, `sceNgsVoicePlay`, `sceNgsVoicePause`,
`sceNgsVoiceKeyOff`.

Two things to be careful about:

- The assertion text is the **game's own**, printed through its TTY. It is not
  an emulator assertion; `patchRouteInfo` is not a symbol in this tree.
- The game's assertion at 00:28:10.899 precedes our first error return at
  00:28:10.899 to 00:28:10.899, so this cluster is not caused by the guards
  added in commits `67b8823f`, `b08c40f8` and `8fc6c007`. Those guards refuse
  a stale handle, which is what they are for, and this is the game reacting
  to a routing failure.

The cluster is one event in a 21 ms window, not a continuous failure, so it
is not the reason for the missing audio either.

## What is not known

- The perf CSVs on the device are stale. `frames.csv` holds 219 rows over
  6.7 s for `PCSA00029`, which is Uncharted, not Jak and Daxter. The
  computed 32.77 FPS is Uncharted and must not be quoted for this title.
- `sceNgsSystemUpdate` being called twice was a miscount, see the second
  session below. Do not repeat that claim.
- No FPS figure for Jak and Daxter was measured. The 18 to 20 is the user's
  observation and is not in any log.

## Second session, same build, longer play

`tmp/play-jak2/vita3k.log`, 918 MB, 6647516 lines, 00:37:02 to 00:49:33.
Still `dev.8`, PID 20915, still running at the end of the log. **No crash in
12 and a half minutes.**

The log rotated, so line 1 starts the session and the whole file is one
session. The earlier "sceNgsSystemUpdate called twice" was a miscount: both
matches were `STALE HOST POINTER` lines that mention the import name, not
calls to it. `sceNgsVoicePlay` is called 2425 times and
`sceNgsPatchCreateRouting` 66 times, so NGS is in use.

### The host audio device is working, so the silence is upstream

`dumpsys media.audio_flinger` while the game was running:

```
Index Active Full Partial Empty  Written
0     yes  363    0       97        361238400
```

361 MB written to track 0, and it is active. **SDL is opening an audio stream
and pushing frames to the device.** The emulator side is not the break, which
moves the question up into NGS: the mixer is not producing samples for the
game to pull.

One mismatch is visible. The active stream reports `44100`, the mixer's own
rate is `48000`, and the NGS resampler log shows `44061 -> 48000 Hz`. Three
rates in one path.

### The resampler is rebuilt 1821 times, each with a different source rate

```
13866 -> 48000 Hz
13872 -> 48000 Hz
13879 -> 48000 Hz
```

The source rate climbs about 6 Hz each time. That is a rate computed from
something that drifts, and a new resampler is built for it each time instead
of reusing one. 1821 creations between 00:37:38 and 00:39:14, a startup burst
rather than a per-frame cost. Wasted work, and a sign the rate is wrong, but
not why the game is silent: the rate never settles on the value asked for.

### The game asserts the channel count 2.7 million times

```
1446977  Assertion failed: patchRouteInfo.nOutputChannels == 2
1248906  Assertion failed: false
```

All 26 guest breakpoints are on `audio_out_thread`, and the last import before
them is `sceKernelUnlockLwMutex2`. The game's audio thread is failing an
assertion in a loop. **This is the audio problem**: the game believes its
routing has the wrong channel count and refuses to play.

**This is not caused by the guards added in this series.** The first assertion
here is at 00:39:28.366 and our first `sceNgsPatchGetInfo` error return is at
00:39:28.367, a millisecond later. In the earlier session the first assertion
is at 23:58:56, before `dev.8` was installed at 00:03, so it predates the
guards entirely.

The `dest->rack ? dest->rack->channels_per_voice : 0` fallback added in
`8fc6c007` does return 0 where the game asserts 2. That is wrong and needs
fixing, but it is not what starts this.

### 15 pipelines fail with ErrorOutOfHostMemory, and that is the vanishing

```
Failed to create pipeline #0 (9 succeeded so far): ErrorOutOfHostMemory
...
Failed to create pipeline #14
```

Between 00:39:28 and 00:47:58, most of the session and not only at startup.
**A pipeline that fails to create is a draw that cannot run, which is exactly
"elements vanishing unexpectedly."** This is the strongest candidate for that
symptom.

`OutOfHostMemory` while the device has 4.5 GB free and the app sits at 1.1 GB
RSS means it is not a system-wide shortage. Either a Vulkan host allocation is
failing against a limit, or fragmentation from the 2560x1440 swapchain with 4
images. Tickets 27 and 28 own parts of this.

### The SPIR-V rejection is separate and smaller than it looked

35 `SPIR-V parsing FAILED` and 43 `spirv_to_nir failed`, all
`OpBitcast must have the same total number of bits`. Fewer than in the first
session and all early. A real translator bug, but it does not explain the
vanishing elements on its own: a rejected shader yields a failed pipeline, not
a silently missing draw.

## What to do next, in order

1. **Channel count.** `nOutputChannels` is asserted 1.4 million times on the
   audio thread. Find what the game expects and return that. This is the audio
   fix.
2. **Pipeline `ErrorOutOfHostMemory`.** 15 draws skipped over 8 minutes. This
   is the vanishing-elements fix. Start with swapchain size and memory limits.
3. **Resampler rate.** 1821 constructions with a drifting source rate.
4. **SPIR-V OpBitcast.** Real, but lower priority than the two above.
