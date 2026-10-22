// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/vertex_stream.h>

#include <gtest/gtest.h>

#include <numeric>
#include <vector>

using namespace renderer::vulkan;

TEST(VertexStream, mixed_unaligned_attributes_preserve_each_vertex) {
    const std::array<VertexStreamAttribute, 2> attributes = { { { 0, 1, 4, 4 }, { 1, 5, 4, 2 } } };
    const auto layout = make_vertex_stream_layout(12, attributes);
    ASSERT_TRUE(layout.repack);
    EXPECT_EQ(layout.offsets[0], 4u);
    EXPECT_EQ(layout.offsets[1], 8u);
    EXPECT_EQ(layout.stride, 12u);
    std::vector<uint8_t> source(21);
    std::iota(source.begin(), source.end(), 1);
    std::vector<uint8_t> output(repacked_vertex_stream_size(source.size(), 12, layout));
    repack_vertex_stream(output, source, 12, layout);
    for (size_t vertex = 0; vertex < 2; vertex++) {
        for (const auto &attribute : attributes) {
            for (size_t byte = 0; byte < attribute.size; byte++)
                EXPECT_EQ(output[vertex * layout.stride + layout.offsets[attribute.index] + byte], source[vertex * 12 + attribute.offset + byte]);
        }
    }
    EXPECT_EQ(output[0], 0);
}

TEST(VertexStream, zero_stride_preserves_a_constant_attribute) {
    const std::array<VertexStreamAttribute, 1> attributes = { { { 0, 1, 4, 4 } } };
    const auto layout = make_vertex_stream_layout(0, attributes);
    ASSERT_TRUE(layout.repack);
    EXPECT_EQ(layout.stride, 0u);
    const std::array<uint8_t, 5> source = { 0, 10, 20, 30, 40 };
    std::vector<uint8_t> output(repacked_vertex_stream_size(source.size(), 0, layout));
    repack_vertex_stream(output, source, 0, layout);
    EXPECT_EQ(output, (std::vector<uint8_t>{ 0, 0, 0, 0, 10, 20, 30, 40 }));
}

TEST(VertexStream, byte_attributes_allow_odd_strides) {
    const std::array<VertexStreamAttribute, 1> attributes = { { { 0, 1, 4, 1 } } };
    EXPECT_FALSE(make_vertex_stream_layout(5, attributes).repack);
    EXPECT_TRUE(make_vertex_stream_layout(5, attributes, true).repack);
}

TEST(VertexStream, final_partial_record_does_not_copy_trailing_bytes) {
    const std::array<VertexStreamAttribute, 1> attributes = { { { 0, 1, 4, 4 } } };
    const auto layout = make_vertex_stream_layout(7, attributes);
    const std::array<uint8_t, 3> source = { 1, 2, 3 };
    std::vector<uint8_t> output(repacked_vertex_stream_size(source.size(), 7, layout), 255);
    repack_vertex_stream(output, source, 7, layout);
    EXPECT_EQ(output, (std::vector<uint8_t>{ 0, 0, 0, 0, 2, 3, 0, 0 }));
}

TEST(VertexStream, overlapping_attributes_keep_shared_bytes) {
    const std::array<VertexStreamAttribute, 2> attributes = { { { 0, 1, 8, 4 }, { 1, 3, 4, 2 } } };
    const auto layout = make_vertex_stream_layout(12, attributes);
    EXPECT_EQ(layout.offsets[1], layout.offsets[0] + 2);
    EXPECT_EQ(layout.copy_count, 1u);
}
