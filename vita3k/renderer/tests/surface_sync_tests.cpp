// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/surface_sync.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace {
TEST(SurfaceSync, presentation_uses_cpu_pixels_after_the_cached_render_is_idle) {
    EXPECT_FALSE(renderer::present_guest_surface(true, 0, 10, 11));
    EXPECT_TRUE(renderer::present_guest_surface(true, 0, 10, 12));
    EXPECT_TRUE(renderer::present_guest_surface(false, 11, 10, 12));
    EXPECT_FALSE(renderer::present_guest_surface(false, 9, 10, 12));
    EXPECT_FALSE(renderer::present_guest_surface(true, 11, 10, 0));
    EXPECT_FALSE(renderer::present_guest_surface(false, 11, 12, 14));
}

TEST(SurfaceSync, partial_rows_preserve_untouched_cpu_bytes) {
    std::array<uint8_t, 40> src, dst;
    src.fill(0xAA);
    dst.fill(0xBB);
    ASSERT_TRUE(renderer::copy_surface_sync_rows(dst.data() + 3, src.data() + 3, 37, 8, 3, 4));
    for (size_t i = 0; i < dst.size(); i++) {
        const bool copied = i >= 3 && i < 30 && (i - 3) % 8 < 3;
        EXPECT_EQ(dst[i], copied ? 0xAA : 0xBB) << i;
    }
}

TEST(SurfaceSync, invalid_rows_do_not_write_any_bytes) {
    std::array<uint8_t, 32> src{}, dst;
    dst.fill(0xBB);
    const auto original = dst;
    EXPECT_FALSE(renderer::copy_surface_sync_rows(dst.data(), src.data(), 32, 8, 9, 2));
    EXPECT_FALSE(renderer::copy_surface_sync_rows(dst.data(), src.data(), 32, 8, 8, 5));
    EXPECT_FALSE(renderer::copy_surface_sync_rows(dst.data(), src.data(), 32, 0, 1, 1));
    EXPECT_FALSE(renderer::copy_surface_sync_rows(dst.data(), src.data(), 32, 8, 8, 0));
    EXPECT_EQ(dst, original);
}

TEST(SurfaceSync, span_excludes_trailing_row_padding_and_does_not_wrap) {
    EXPECT_EQ(renderer::surface_sync_rows_span(64, 8, 3), 136);
    const uint32_t max = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(renderer::surface_sync_rows_span(max, max, max), static_cast<uint64_t>(max) * max);
}

TEST(SurfaceSync, drawn_rectangle_is_intersected_with_the_bound_surface) {
    const auto rect = renderer::intersect_surface_rects({ 3, 4, 90, 80 }, { 0, 0, 32, 24 });
    EXPECT_EQ(rect.x0, 3);
    EXPECT_EQ(rect.y0, 4);
    EXPECT_EQ(rect.x1, 32);
    EXPECT_EQ(rect.y1, 24);
    EXPECT_FALSE(rect.empty());
    EXPECT_TRUE(renderer::intersect_surface_rects(rect, { 40, 0, 64, 24 }).empty());
}
} // namespace
