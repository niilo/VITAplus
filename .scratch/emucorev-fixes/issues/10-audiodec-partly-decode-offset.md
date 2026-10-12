# 10: sceAudiodecPartlyDecode applies the sample offset twice

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceAudiodec/SceAudiodecUser.cpp:386`:

```cpp
memcpy(pcm_data + samples_offset * bytes_per_sample, temp_storage.data() + samples_offset * bytes_per_sample, samples_to_decode * bytes_per_sample);
```

`pcm_data` is the guest pointer the caller passed. It already points at the
sample the caller wants written, which is why the decode was started at
`samples_offset` in `temp_storage`. Adding the offset again on the destination
writes `samples_offset * bytes_per_sample` bytes past where the guest expects
them.

The size is also wrong. The decoded range starts at `temp_storage +
samples_offset` and the destination starts at `pcm_data`, so the length should
be `samples_to_decode * bytes_per_sample` starting at each, which is what the
call does. The defect is only the destination offset.

When `samples_offset` is 0, which is the common case, the copy is correct.
Every title that decodes into the middle of a buffer is affected.

EmuCoreV commit `5e4f7c58` change 9 changes one line.

## Plan

1. Change the destination to `pcm_data` and keep the source offset:
   `memcpy(pcm_data, temp_storage.data() + samples_offset * bytes_per_sample, samples_to_decode * bytes_per_sample);`
2. Read the whole function and confirm `temp_storage` is sized and filled
   for the range that is copied. `temp_storage.resize(old_size + pcm_size_given)`
   at `:382` suggests the decode appends, so check that the source range is
   inside the vector for every accepted combination of `samples_offset` and
   `samples_to_decode`.
3. Check the other decode entry points in the same file for the same doubled
   offset. Record what you find as a comment here.

## Comments

### 2026-10-12: Vita3K-Plus has the same one-line change

Vita3K-Plus commit `e2331f80` changes the same line, and names the title it
fixes: PCSG00841, a crash on the initial screen. That is the cheapest item in
the whole comparison.