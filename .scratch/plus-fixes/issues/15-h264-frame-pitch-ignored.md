# 15: The H.264 decoder writes rows at frameWidth and ignores framePitch

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/codec/src/h264.cpp:28`:

```cpp
void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest, const uint32_t width, const uint32_t height, bool is_p3) {
    for (size_t i = 0; i < height; i++) {
        memcpy(dest, &frame->data[0][frame->linesize[0] * i], width);
        dest += width;
    }
```

The source pitch is honoured through `frame->linesize`. The destination pitch is
`width`, because `dest += width`.

The destination is the guest's picture buffer, allocated by the guest with
`pPicture->frame.framePitch` as the row stride. That field is already in the
struct at `vita3k/modules/SceVideodec/SceVideodecUser.cpp:124` and nothing
reads it.

When `framePitch > frameWidth` the emulator packs rows at `frameWidth` and the
guest reads them at `framePitch`, so the luma plane shears and the chroma planes
land at the wrong offset. The output is a skewed picture with colour bands.

Both call sites drop the pitch: `vita3k/codec/src/h264.cpp:81` and
`vita3k/modules/SceVideodec/SceVideodecUser.cpp:229` and `:386`.

Vita3K-Plus commit `44775966`, found on Hustle Kings and not a per-title bug.

## Plan

1. Add a `pitch_in` parameter to `copy_yuv_data_from_frame` and use it for
   `dest += pitch` on every plane. Pass `pPicture->frame.framePitch` from both
   call sites.
2. Add a `frame_bytes()` helper that computes the buffer size from the pitch,
   not from width times height.
3. `take_held_picture` at `h264.cpp:333` compares
   `it->data.size() != buffer_size({ { width, height } })`. That predicate has to
   use the pitch as well, or a held picture is silently rejected and the frame
   is decoded twice.
4. Check `H264DecoderState::buffer_size` at `h264.cpp:95`, which is still
   `width * height * 3 / 2` and is used for `ctrl->pcm_size_max`. Decide whether
   it should be pitch-based. Plus leaves it alone, which is right only if the
   guest's buffer is sized by pitch.
5. Test with a title that sets `framePitch` above `frameWidth`, which is the
   common case for video.

## Comments