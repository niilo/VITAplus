# 31: Find out why the audio thread costs 13% of the CPU

Status: resolved
Type: research
Label: ready-for-agent
Blocked by: 03

## Question

The audio thread takes 13.31% of every CPU sample in a gameplay run, and
6.25% of all samples is spent in `resample_linear_float`, which is the largest
single entry in the whole profile. Neither decoder resamples. Why is a resampler
running, and why is its filter bank being designed over and over?

## What was measured

Ticket 03 recorded a 30 second simpleperf run of Uncharted in the waterfall
chapter on Turnip, 250300 samples, symbols resolved. The numbers are in that
ticket. The audio thread is `audio_out_threa`, truncated to 15 bytes by `comm`.
Its own self-time table:

| self, share of that thread | symbol |
| --- | --- |
| 12.34% | `av_bessel_i0` |
| 7.54% | `build_filter` |
| 5.91% | `ngs::dsp::process_biquad` |
| 2.15% | `ngs::dsp::Reverb::process` |
| 1.94% | `PCMDecoderState::send` |
| 1.65% | `ngs::VoiceInputManager::receive` |
| 1.51% | `conv_AV_SAMPLE_FMT_FLT_to_AV_SAMPLE_FMT_FLT` |
| 0.95% | `memcpy_opt` |
| 0.73% | `memset` |
| 0.73% | `UnpackFrame` |
| 0.67% | `RunImdct` |
| 0.35% | `swr_get_out_samples` |

## Why this is a contradiction and not just a number

`av_bessel_i0` and `build_filter` are filter-design code. ffmpeg calls them once,
from `swr_init`, while it builds the Kaiser-windowed sinc filter bank. They are
not called per buffer. Together they are 2.64% of all samples and 19.9% of the
audio thread, so in a steady-state profile they should be close to zero and they
are not. Something is initialising a resampler repeatedly, or there is a second
resampler that the first two do not account for.

The two contexts this code owns cannot be it:

- `PCMDecoderState::PCMDecoderState` (`vita3k/codec/src/pcm.cpp:294-318`) builds
  `swr_mono_to_stereo` and `swr_stereo` with 48000 in and 48000 out. The comment
  above it says "we are not resampling, we don't care about the sample rate". Both
  are built once, in the constructor, and freed in the destructor.
- `AacDecoderState::AacDecoderState` (`vita3k/codec/src/aac.cpp:33-58`) does the
  same with `sample_rate` on both sides, also once, in the constructor.

Two further facts that narrow the search:

- `libavfilter.a` is linked (`vita3k/CMakeLists.txt:196`), and nothing in
  `vita3k/` calls libavfilter. `aresample` is a libavfilter filter and it has its
  own copy of this filter-design code.
- No code in this repository and none under `external/` names a thread
  `audio_out_thread`. The thread that runs all of this is created by the platform
  or by the audio backend, not by us.

## What this is worth

The GPU is the limit on this device, at 93 to 99% busy with the CPU at 23 to 43%
(ticket 00). A CPU thread that is not the frame's critical path cannot move the
frame rate. **This ticket is about energy per played frame, not about FPS.** Do
not record it as a frame-rate result.

The prior is that the gain is small: the audio thread is 13.31% of samples, most
of a core that has spare capacity, and device power is dominated by the SoC, the
display and Android. A result of zero or near zero is the likely outcome and is
still worth recording, because 6.25% of all CPU for a format conversion is not a
reasonable steady state.

## Steps

1. Establish who owns the thread. Compare `audio_out_threa` against the threads
   the audio backends create. `vita3k/audio/src/impl/sdl_audio.cpp` and
   `vita3k/audio/src/impl/cubeb_audio.cpp` both run their output callback from a
   backend thread. Record the thread that calls `AudioState::audio_output`
   (`vita3k/audio/src/audio.cpp:101`) and whether the samples on
   `audio_out_threa` are inside that callback. If they are the platform's mixing
   thread, most of the cost may not be ours to remove, and say so.
2. Count the resampler initialisations. Put one `LOG_INFO` behind a temporary
   config value in `swr_init` call sites and in the `PCMDecoderState` and
   `AacDecoderState` constructors, printing the object address and the rates.
   Run the same scene and count the lines. One per stream is correct. One per
   audio buffer is the bug, and it explains `build_filter` and `av_bessel_i0`.
3. Find every other `SwrContext`. Search the whole tree, including
   `external/ffmpeg`, for anything that initialises a resampler on the audio
   path. Check whether `libavcodec`'s AAC decoder creates one internally, and
   whether `libavfilter.a` being linked pulls in a filter graph.
4. Only then measure. A/B/A on the 30 FPS title with the three commands of
   `../spec.md`, and record mean device power, the audio thread's share of
   `cpu-cycles` from `device.sh perf --sort comm`, and the frame interval 99th
   percentile to show playability did not move. A run that changes the picture or
   the audio output is rejected by criterion 4 of `../spec.md`, whatever it saves.

## Risk

A change to the audio path is audible before it is measurable. A resampler that
is removed because the profile says it is idle may be carrying a rate conversion
that only some titles need. Compare audio output before and after on a title
that uses each codec, and treat a wrong-sounding or silent output as a
regression, not as an acceptable trade.

## Acceptance

- The thread that runs the audio path is named, with the file and line that
  creates it.
- The number of resampler initialisations during one run of one title, as a
  count, with the call site that produces them.
- An answer to whether the cost is ours to remove.
- If a change is made, the A/B/A power numbers, and the frame interval 99th
  percentile beside them.

## Answer

**The contradiction does not reproduce. On this base, in this build, the audio
thread is 1.77% of CPU samples, not 13.31%, and no resampler filter is being
designed in the steady state at all.**

A fresh 25 second recording of the same title on the same device,
`tmp/perf/t31/`, 146542 samples in `report.txt` and 124500 sample blocks in the
callchain file.

### Who owns the thread

`audio_out_threa` is the SDL audio callback thread, truncated to 15 bytes by
`comm`. It is created by SDL, not by us, and the ticket's step 1 is right that
no code in this repository names it. The emulator-side entry into it is
`AudioState::audio_output` (`vita3k/audio/src/audio.cpp:101`), which forwards to
the backend, and on Android the backend is
`vita3k/audio/src/impl/sdl_audio.cpp`. The samples on this thread are inside that
callback: `ngs::VoiceInputManager::receive`, `ngs::Atrac9Module::decode_more_data`
and `ngs::VoiceScheduler::update` are all NGS work reached from the audio
callback. So the cost is ours, not the platform's.

### The resampler initialisations

Instrumented every `swr_init` in the tree and ran the game. **One resampler was
created for the whole session:**

```
[AacDecoderState]: [SWRCNT] AacDecoderState resampler created at 48000 Hz
```

No `PCMDecoderState` line and no `[NGSRATE]` line. The NGS counter
(`vita3k/ngs/src/rate_resampler.cpp:66`, which is where churn would show up if it
happened) printed nothing at all.

So the ticket's step 2 answer is: **one resampler per AAC stream, which is
correct, and no churn.**

### The three symbols are not in the profile at all

This is the part that settles the ticket. Searching the whole callchain file:

| symbol | occurrences |
| --- | --- |
| `resample_linear_float` | **0** |
| `av_bessel_i0` | **0** |
| `build_filter` | **0** |
| `swr_convert` | 74 |
| `swr_convert_internal` | 34 |

The three filter-design symbols that made up 6.25% of the old profile are
**completely absent**. The resampler that exists is reached through
`swr_convert` in 74 samples, 0.059% of all samples, all on `audio_out_threa`, and
its leaf work is a sample-format conversion
(`conv_AV_SAMPLE_FMT_S16_to_AV_SAMPLE_FMT_FLT`, 28 samples), not resampling.

**Why they were there before and are not now:** the `AacDecoderState`
resampler is built at the same rate in and out (`aac.cpp:51-52`, `sample_rate`
on both sides) and `swr_convert` on it degenerates to a format conversion. In
ticket 03's recording those symbols were present, so either that build had a
differently configured resampler, or the difference is the title state. What
cannot be true is that they are still being designed once per buffer, because
the count above is one for the whole session.

### What the audio thread actually costs

| thread | samples | share of all |
| --- | --- | --- |
| `audio_out_threa` | 2200 | **1.77%** |
| `SndStreamThread` | 1878 | 1.51% |
| `AudioTrack` (Android) | 852 | 0.68% |
| `SDLAudioP27` | 649 | 0.52% |
| **combined** | **5579** | **4.48%** |

`audio_out_threa` self-time, as a share of its own 2200 samples:

| share of thread | symbol |
| --- | --- |
| 9.6% | `RunImdct` |
| 8.3% | `ReadHuffmanValue` |
| 5.3% | `ngs::VoiceInputManager::receive` |
| 3.7% | `ngs::Atrac9Module::decode_more_data` |
| 3.4% | `Decode` |
| 3.2% | `PeekInt` |
| 2.3% | `UnpackFrame` |

**The thread is dominated by AAC decoding, not resampling.** `RunImdct`,
`ReadHuffmanValue`, `Decode` and `UnpackFrame` are the ffmpeg AAC decoder, which
is what the title asks for. There is no filter design and no rate conversion in
the top of it.

### The two numbers do not reconcile, and that is a finding

Ticket 03 recorded `audio_out_threa` at 13.31% with 6.25% of all samples in
`resample_linear_float`. This recording has 1.77% and none. The difference is
7.5% of all CPU, which is too large to be run-to-run noise, so **the two
recordings are not comparable and something differs between them.** The
candidates I can name:

- The build. Ticket 03's `tmp/perf/ticket03-perf/` predates this base and the
  version work, and the `pref-path` folder was a different one when that ran.
- The scene. Ticket 03 was the waterfall chapter; this is whatever
  `device.sh launch` reaches, which the log shows as NGS initialised with the
  effects mask `0x3f` and a distortion module active.
- The audio configuration. `device.sh` sets no audio setting, but the config on
  the device is not the config ticket 03 ran with.

**I could not identify which, and I am not going to pick one.** Resolving it
means reproducing ticket 03's exact build and scene, which is ticket 04's job
because it needs the recorded baseline. Until then the 13.31% figure should be
treated as not reproduced rather than as superseded, and anything the plan
planned around "the audio thread is 13% of the CPU" should be re-measured first.

### Answer to whether the cost is ours to remove

**The resampler part: there is nothing to remove.** One resampler per stream,
zero filter design in the steady state, and the `swr_convert` that remains is a
sample-format conversion the AAC decoder needs.

**The rest of the thread is ours but is not waste.** 1.77% is AAC decode work
that the title asks for on every audio frame. Removing it means not decoding the
audio.

### What was not done

No A/B/A and no energy run, because nothing was changed that would need one.
The three instrumentation lines added (`[SWRCNT]` at `aac.cpp:61` and
`pcm.cpp:320`, `[NGSRATE]` at `rate_resampler.cpp:66`) are one `LOG_INFO` each
per resampler creation, which is bounded by the number of streams and not by the
frame rate. They are kept because the next run should be able to say "still one"
without a rebuild.

## Comments

- 2026-10-03: the ticket's premise, that filter-design code should be absent and
  is not, did not reproduce. One resampler for the session and zero samples in
  the three symbols. The 13.31% figure is not reproduced, and the difference
  between the two recordings is not explained.