# 08: NGS PlayerModule copies a guest-supplied channel count into a two-element array

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/ngs/src/modules/player.cpp:250-256`:

```cpp
runtime->decoder->source_channels = params->channels;
runtime->decoder->source_frequency = params->playback_frequency;
// Enable ADPCM mode on the decoder as needed, and restore state
runtime->decoder->he_adpcm = static_cast<bool>(params->type);
if (runtime->decoder->he_adpcm) {
    std::copy_n(logical->adpcm_history, runtime->decoder->source_channels, runtime->decoder->adpcm_history);
}
```

`params->channels` is an `SceInt8` read from guest memory
(`vita3k/ngs/include/ngs/modules/player.h:62`), so it can be up to 127.

The source is `logical->adpcm_history[SCE_NGS_PLAYER_MAX_PCM_CHANNELS]`
(`player.h:85`), which is 2 entries at `player.h:38`. The destination is
`ADPCMHistory adpcm_history[2]` in the decoder
(`vita3k/codec/include/codec/state.h:236`).

So the copy writes up to 125 elements past a two-element array, from a guest
value, on the audio thread. The reverse copy at `:340` has the same shape.

Vita3K-Plus commit `7451f9f4`, from Vita3K-nx.

## Plan

1. Reject a `channels` value outside `0 .. SCE_NGS_PLAYER_MAX_PCM_CHANNELS`
   before either copy. Finish the voice rather than continuing, which is what
   the player loop already does for an unusable buffer.
2. Log the first sixteen occurrences, so a title that trips this is visible.
   The NGS modules already have a `LOG_*_ONCE` pattern.
3. Apply the same bound to the reverse copy at `:340`.
4. Audit `vita3k/ngs/src/modules/atrac9.cpp` for the same shape: the agent
   report places an unchecked `next_buffer_index` from the guest at `:110`.
   Record what you find here.
5. Test: set `channels` to 8 with ADPCM on, and check that nothing outside the
   two-element array changed. The NGS DSP tests in `vita3k/ngs/tests/` are the
   right place.

## Comments