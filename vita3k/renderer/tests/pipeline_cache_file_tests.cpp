// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/vulkan/pipeline_cache_file.h>

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {
using namespace renderer::vulkan;
constexpr size_t prefix_size = sizeof(uint32_t) + sizeof(size_t);
constexpr size_t driver_offset = prefix_size + 2 * sizeof(uint64_t);
const PipelineCacheIdentity gpu{ 0x5143, 0x43050a00, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } };

template <typename T>
void put(std::vector<uint8_t> &bytes, size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::vector<uint8_t> cache() {
    std::vector<uint8_t> bytes(driver_offset + 40, 0xab);
    put(bytes, 0, pipeline_cache_magic);
    put(bytes, sizeof(uint32_t), size_t{ 2 });
    put(bytes, prefix_size, uint64_t{ 0x123456789abcdef0 });
    put(bytes, prefix_size + sizeof(uint64_t), uint64_t{ 0xfedcba9876543210 });
    put(bytes, driver_offset, uint32_t{ 32 });
    put(bytes, driver_offset + 4, uint32_t{ 1 });
    put(bytes, driver_offset + 8, gpu.vendor);
    put(bytes, driver_offset + 12, gpu.device);
    std::memcpy(bytes.data() + driver_offset + 16, gpu.uuid.data(), gpu.uuid.size());
    return bytes;
}

TEST(PipelineCacheFile, accepts_matching_driver_data_and_unaligned_hashes) {
    const auto bytes = cache();
    const auto view = read_pipeline_cache_file(bytes, gpu);
    ASSERT_TRUE(view);
    EXPECT_EQ(view->hash(0), 0x123456789abcdef0u);
    EXPECT_EQ(view->hash(1), 0xfedcba9876543210u);
    EXPECT_EQ(view->data.size(), 40u);
    EXPECT_EQ(view->data[39], 0xab);
}

TEST(PipelineCacheFile, rejects_every_truncated_prefix_and_driver_header) {
    const auto bytes = cache();
    for (size_t size = 0; size < driver_offset + 32; size++)
        EXPECT_FALSE(read_pipeline_cache_file(std::span(bytes).first(size), gpu)) << size;
}

TEST(PipelineCacheFile, rejects_hash_counts_that_would_overflow_or_overrun) {
    for (const size_t count : { std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max() / 8 + 2, size_t{ 100 } }) {
        auto bytes = cache();
        put(bytes, sizeof(uint32_t), count);
        EXPECT_FALSE(read_pipeline_cache_file(bytes, gpu));
    }
}

TEST(PipelineCacheFile, rejects_another_vendor_device_or_driver_uuid) {
    const auto bytes = cache();
    auto other = gpu;
    other.vendor ^= 1;
    EXPECT_FALSE(read_pipeline_cache_file(bytes, other));
    other = gpu;
    other.device ^= 1;
    EXPECT_FALSE(read_pipeline_cache_file(bytes, other));
    other = gpu;
    other.uuid[15] ^= 1;
    EXPECT_FALSE(read_pipeline_cache_file(bytes, other));
}

TEST(PipelineCacheFile, rejects_invalid_header_lengths_versions_and_magic) {
    for (uint32_t size : { 0u, 31u, 41u, std::numeric_limits<uint32_t>::max() }) {
        auto bytes = cache();
        put(bytes, driver_offset, size);
        EXPECT_FALSE(read_pipeline_cache_file(bytes, gpu));
    }
    auto bytes = cache();
    put(bytes, driver_offset + 4, uint32_t{ 2 });
    EXPECT_FALSE(read_pipeline_cache_file(bytes, gpu));
    bytes = cache();
    put(bytes, 0, uint32_t{ 0 });
    EXPECT_FALSE(read_pipeline_cache_file(bytes, gpu));
}

TEST(PipelineCacheFile, accepts_a_matching_cache_with_no_pipeline_keys) {
    auto bytes = cache();
    bytes.erase(bytes.begin() + prefix_size, bytes.begin() + driver_offset);
    put(bytes, sizeof(uint32_t), size_t{ 0 });
    const auto view = read_pipeline_cache_file(bytes, gpu);
    ASSERT_TRUE(view);
    EXPECT_TRUE(view->hashes.empty());
    EXPECT_EQ(view->data.size(), 40u);
}
} // namespace
