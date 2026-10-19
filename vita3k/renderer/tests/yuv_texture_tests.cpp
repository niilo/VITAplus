// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/functions.h>
#include <renderer/texture_cache.h>

#include <gtest/gtest.h>

extern "C" {
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <vector>

namespace {
TEST(YuvTexture, yvu_and_yuv_plane_orders_produce_the_same_rgb_pixels) {
    constexpr uint32_t width = 16, height = 16;
    constexpr size_t y_size = width * height, uv_size = y_size / 4;
    std::vector<uint8_t> yuv(y_size + 2 * uv_size + 64, 81), yvu = yuv;
    std::fill_n(yuv.begin() + y_size, uv_size, 90);
    std::fill_n(yuv.begin() + y_size + uv_size, uv_size, 240);
    std::fill_n(yvu.begin() + y_size, uv_size, 240);
    std::fill_n(yvu.begin() + y_size + uv_size, uv_size, 90);
    std::vector<uint8_t> first(y_size * 4 + 64), second(first.size());
    renderer::YUVConversionCache cache;
    renderer::texture::yuv420_texture_to_rgb(cache, first.data(), yuv.data(), width, height, width, height, true, false);
    renderer::texture::yuv420_texture_to_rgb(cache, second.data(), yvu.data(), width, height, width, height, true, true);
    EXPECT_EQ(first, second);
    EXPECT_GT(first[0], first[2]);
    sws_freeContext(static_cast<SwsContext *>(cache.sws_context));
}
} // namespace
