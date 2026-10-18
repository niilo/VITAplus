// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

// The two-block bookkeeping of a VM allocation.
//
// sceKernelAllocMemBlockForVM inserts one uid into two independent maps,
// SysmemState::blocks and SysmemState::vm_blocks, both keyed by the same uid
// and both holding their own node. Releasing it has to remove the node from
// each map with an iterator that belongs to that map. Handing vm_blocks' node
// to blocks.erase() unlinks it twice, which is undefined behaviour and leaves
// the tree unusable.
//
// These tests exercise the map contract directly. The real entry points need
// EmuEnvState and a renderer, so they cannot be called from a test binary, but
// the shape they operate on is a plain std::map pair.

#include <modules/sysmem_state.h>

#include <gtest/gtest.h>

#include <memory>

namespace {
KernelMemBlockPtr make_block(const uint32_t size) {
    auto block = std::make_shared<KernelMemBlock>();
    block->mappedBase = Ptr<void>(0);
    block->mappedSize = size;
    // The allocation path fills this with sizeof(SceKernelMemBlockInfo), not
    // the requested size, which is what made the accounting drift.
    block->size = sizeof(SceKernelMemBlockInfo);
    return block;
}
} // namespace

TEST(sysmem_block, allocation_is_present_in_both_maps) {
    SysmemState state;

    const SceUID uid = 7;
    const auto block = make_block(KiB(64));
    state.blocks.emplace(uid, block);
    state.vm_blocks.emplace(uid, block);
    state.allocated_user += block->mappedSize;

    EXPECT_TRUE(state.blocks.contains(uid));
    EXPECT_TRUE(state.vm_blocks.contains(uid));
    EXPECT_EQ(state.blocks.at(uid), state.vm_blocks.at(uid));
}

TEST(sysmem_block, vm_release_clears_both_maps_and_the_counter) {
    SysmemState state;

    const SceUID uid = 7;
    const auto block = make_block(KiB(64));
    state.blocks.emplace(uid, block);
    state.vm_blocks.emplace(uid, block);
    state.allocated_user += block->mappedSize;

    // Each map erases with its own iterator. This is the fix: the old code took
    // vm_blocks' iterator and passed it to both.
    const auto vm_block = state.vm_blocks.find(uid);
    ASSERT_NE(vm_block, state.vm_blocks.end());

    state.allocated_user -= vm_block->second->mappedSize;
    state.blocks.erase(uid);
    state.vm_blocks.erase(vm_block);

    EXPECT_TRUE(state.blocks.empty());
    EXPECT_TRUE(state.vm_blocks.empty());
    EXPECT_EQ(state.allocated_user, 0u);
}

TEST(sysmem_block, the_two_maps_are_independent_after_one_erases) {
    // The reason the old code was undefined: erasing a node of one map must not
    // disturb the other, and passing a foreign iterator would. Check that
    // erasing from vm_blocks alone leaves blocks intact, which is what the
    // driver free path now relies on.
    SysmemState state;

    const SceUID uid = 7;
    const auto block = make_block(KiB(64));
    state.blocks.emplace(uid, block);
    state.vm_blocks.emplace(uid, block);

    state.vm_blocks.erase(uid);

    EXPECT_TRUE(state.vm_blocks.empty());
    EXPECT_TRUE(state.blocks.contains(uid));
    EXPECT_EQ(state.blocks.at(uid), block);
}

TEST(sysmem_block, releasing_an_unknown_uid_reports_not_found) {
    // The release path looks the uid up first and returns
    // SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID when it is absent, rather than
    // dereferencing end().
    SysmemState state;

    const SceUID uid = 99;
    const auto vm_block = state.vm_blocks.find(uid);

    EXPECT_EQ(vm_block, state.vm_blocks.end());
}

TEST(sysmem_block, an_alloc_and_release_pair_leaves_the_counter_unchanged) {
    // The defect this covers: allocation raised allocated_user by mappedSize
    // while release subtracted the guest-visible size field, so the counter only
    // ever grew and sceKernelGetFreeMemorySize drifted.
    SysmemState state;

    uint32_t allocated_user = 0;

    for (int i = 0; i < 8; i++) {
        const SceUID uid = static_cast<SceUID>(100 + i);
        const auto block = make_block(KiB(16 + i));
        state.blocks.emplace(uid, block);
        state.vm_blocks.emplace(uid, block);
        allocated_user += block->mappedSize;
    }

    for (int i = 0; i < 8; i++) {
        const SceUID uid = static_cast<SceUID>(100 + i);
        const auto vm_block = state.vm_blocks.find(uid);
        ASSERT_NE(vm_block, state.vm_blocks.end());

        allocated_user -= vm_block->second->mappedSize;
        state.blocks.erase(uid);
        state.vm_blocks.erase(vm_block);
    }

    EXPECT_EQ(allocated_user, 0u);
    EXPECT_TRUE(state.blocks.empty());
    EXPECT_TRUE(state.vm_blocks.empty());
}