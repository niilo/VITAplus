# 31: Find out why the audio thread costs 13% of the CPU

Status: open
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

## Comments