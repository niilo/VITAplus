// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mem/mempool.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {
constexpr uint32_t allocation_failed = 0xffffffff;

void expect_partition(const MemspaceBlockAllocator &pool, uint32_t size) {
    uint32_t next = 0;
    for (const auto &block : pool.blocks) {
        EXPECT_EQ(block.offset, next);
        EXPECT_GT(block.size, 0u);
        next += block.size;
    }
    EXPECT_EQ(next, size);
}

TEST(mempool, first_split_creates_only_the_allocated_block_and_remainder) {
    MemspaceBlockAllocator pool(64);
    ASSERT_EQ(pool.alloc(4), 0u);
    ASSERT_EQ(pool.blocks.size(), 2u);
    EXPECT_FALSE(pool.blocks[0].free);
    EXPECT_TRUE(pool.blocks[1].free);
    expect_partition(pool, 64);
}

TEST(mempool, repeated_splits_keep_a_complete_ordered_partition) {
    MemspaceBlockAllocator pool(256);
    for (uint32_t offset = 0; offset < 256; offset += 4) {
        ASSERT_EQ(pool.alloc(4), offset);
        expect_partition(pool, 256);
    }
    EXPECT_EQ(pool.blocks.size(), 64u);
    EXPECT_EQ(pool.alloc(4), allocation_failed);
    expect_partition(pool, 256);
}

TEST(mempool, every_allocation_can_be_freed_and_reused_after_vector_growth) {
    MemspaceBlockAllocator pool(256);
    std::vector<uint32_t> offsets;
    for (uint32_t i = 0; i < 64; ++i)
        offsets.push_back(pool.alloc(4));
    for (auto offset : offsets)
        EXPECT_TRUE(pool.free(offset));
    for (auto offset : offsets)
        EXPECT_EQ(pool.alloc(4), offset);
    expect_partition(pool, 256);
}

TEST(mempool, alignment_and_exact_exhaustion_preserve_the_pool) {
    MemspaceBlockAllocator pool(16);
    EXPECT_EQ(pool.alloc(1), 0u);
    EXPECT_EQ(pool.alloc(5), 4u);
    EXPECT_EQ(pool.alloc(3), 12u);
    EXPECT_EQ(pool.alloc(1), allocation_failed);
    expect_partition(pool, 16);
    EXPECT_FALSE(pool.free(2));
    EXPECT_TRUE(pool.free(4));
    EXPECT_EQ(pool.alloc(8), 4u);
    expect_partition(pool, 16);
}

TEST(mempool, guest_pointer_wrapper_preserves_offsets_and_reuses_freed_blocks) {
    constexpr Address base = 0x10000000;
    MempoolObject pool(Ptr<void>(base), 16);
    const auto first = pool.alloc_raw(8);
    const auto second = pool.alloc_raw(8);
    EXPECT_EQ(first.address(), base);
    EXPECT_EQ(second.address(), base + 8);
    EXPECT_FALSE(pool.alloc_raw(4));
    EXPECT_TRUE(pool.free_raw(second));
    EXPECT_EQ(pool.alloc_raw(8).address(), base + 8);
    expect_partition(pool.allocator, 16);
}
} // namespace
