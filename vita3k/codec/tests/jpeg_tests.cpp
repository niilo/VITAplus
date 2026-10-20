// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <codec/state.h>

#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <vector>

namespace {
std::vector<uint8_t> red_image(uint32_t width, uint32_t height, bool bgra = false) {
    std::vector<uint8_t> rgba(width * height * 4 + AV_INPUT_BUFFER_PADDING_SIZE);
    for (size_t i = 0; i < width * height * 4; i += 4) {
        rgba[i + (bgra ? 2 : 0)] = 255;
        rgba[i + 3] = 255;
    }
    return rgba;
}

TEST(JpegConversion, hardware_pitch_preserves_pixels_in_every_plane) {
    constexpr uint32_t width = 18, height = 16;
    const auto rgba = red_image(width, height);
    for (const DecoderColorSpace space : { COLORSPACE_YUV420P, COLORSPACE_YUV422P }) {
        uint32_t compact_pitch[3], compact_offset[3], hw_pitch[3], hw_offset[3];
        get_yuv_frame_layout(width, height, space, false, compact_pitch, compact_offset);
        get_yuv_frame_layout(width, height, space, true, hw_pitch, hw_offset);
        EXPECT_EQ(hw_pitch[0], 32u);
        EXPECT_EQ(hw_pitch[1], 16u);
        EXPECT_EQ(hw_offset[1], 32u * height);
        std::vector<uint8_t> compact(4096, 0xa5), hardware(4096, 0x5a);
        convert_rgb_to_yuv(rgba.data(), compact.data(), width, height, space, width, false, false);
        convert_rgb_to_yuv(rgba.data(), hardware.data(), width, height, space, width, false, true);
        for (uint32_t plane = 0; plane < 3; plane++) {
            const uint32_t rows = plane && space == COLORSPACE_YUV420P ? height / 2 : height;
            const uint32_t columns = plane ? width / 2 : width;
            for (uint32_t row = 0; row < rows; row++) {
                for (uint32_t col = 0; col < columns; col++) {
                    EXPECT_EQ(compact[compact_offset[plane] + row * compact_pitch[plane] + col],
                        hardware[hw_offset[plane] + row * hw_pitch[plane] + col]);
                }
            }
        }
    }
}

TEST(JpegConversion, rgba_and_bgra_produce_the_same_yuv_for_the_same_colour) {
    const auto rgba = red_image(18, 16);
    const auto bgra = red_image(18, 16, true);
    std::vector<uint8_t> from_rgba(4096), from_bgra(4096);
    convert_rgb_to_yuv(rgba.data(), from_rgba.data(), 18, 16, COLORSPACE_YUV420P, 18, false, true);
    convert_rgb_to_yuv(bgra.data(), from_bgra.data(), 18, 16, COLORSPACE_YUV420P, 18, true, true);
    EXPECT_EQ(from_rgba, from_bgra);
}

TEST(JpegConversion, jpeg_crop_uses_the_original_plane_offsets_and_pitch) {
    constexpr uint32_t width = 18, height = 16;
    auto rgba = red_image(width, height);
    for (size_t i = width * 8 * 4; i < width * height * 4; i += 4) {
        rgba[i] = 0;
        rgba[i + 2] = 255;
    }
    for (bool hardware_pitch : { false, true }) {
        std::vector<uint8_t> yuv(4096), jpeg(16384), decoded(4096);
        convert_rgb_to_yuv(rgba.data(), yuv.data(), width, height, COLORSPACE_YUV420P, width, false, hardware_pitch);
        const int bytes = convert_yuv_to_jpeg(yuv.data(), jpeg.data(), width, height, 8, 8, hardware_pitch, jpeg.size(), COLORSPACE_YUV420P, 64);
        ASSERT_GT(bytes, 0);
        MjpegDecoderState decoder;
        ASSERT_TRUE(decoder.send(jpeg.data(), bytes));
        DecoderSize size{};
        ASSERT_TRUE(decoder.receive(decoded.data(), &size));
        EXPECT_EQ(decoder.context->width, 8);
        EXPECT_EQ(decoder.context->height, 8);
        MJpegPitch pitch[4];
        decoder.get_pitch_info(pitch);
        const size_t u = pitch[0].x * pitch[0].y;
        const size_t v = u + pitch[1].x * pitch[1].y;
        EXPECT_LT(decoded[u], 120); // Red has low U and high V.
        EXPECT_GT(decoded[v], 180);
    }
}

TEST(JpegConversion, odd_sized_planes_use_rounded_up_chroma_dimensions) {
    uint32_t pitch[3], offset[3];
    get_yuv_frame_layout(17, 9, COLORSPACE_YUV420P, false, pitch, offset);
    EXPECT_EQ(pitch[1], 9u);
    EXPECT_EQ(offset[1], 153u);
    EXPECT_EQ(offset[2], 198u);
}
} // namespace
