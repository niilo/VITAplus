// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Ptr<T>::get() resolves through the page table on every call, so a guest
// address survives an external GPU mapping. A host pointer taken before the
// mapping does not, and reads as zero. NGS stored its Voice-to-Rack and
// Rack-to-System links as host pointers, which is why a live rack came to read
// as a released one. See .scratch/cpu-hotpath-counters/issues/09.
//
// These tests need no device and no emulator: add_external_mapping() and
// remove_external_mapping() are the whole mechanism.

#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/state.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t PAGE = 4096;
constexpr uint32_t PAGES = 4;

// A host buffer with the size and the alignment add_external_mapping needs.
class AlignedBuffer {
public:
    AlignedBuffer()
        : ptr_(static_cast<uint8_t *>(std::aligned_alloc(PAGE, PAGES * PAGE))) {
        if (ptr_)
            std::memset(ptr_, 0, PAGES * PAGE);
    }
    ~AlignedBuffer() {
        std::free(ptr_);
    }
    AlignedBuffer(const AlignedBuffer &) = delete;
    AlignedBuffer &operator=(const AlignedBuffer &) = delete;

    uint8_t *get() const {
        return ptr_;
    }

private:
    uint8_t *ptr_;
};

struct Payload {
    uint32_t magic;
    uint32_t counter;
    uint64_t wide;
};

// One emulator memory for the file. init() reserves 4 GiB of address space and
// installs one process-wide access violation handler that closes over the
// MemState, so the state has to outlive every test that uses it.
MemState &shared_state() {
    static MemState state;
    return state;
}

class ExternalMappingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(init(shared_state(), true));
        addr_ = alloc(shared_state(), PAGES * PAGE, "external mapping test");
        ASSERT_NE(addr_, 0);
    }

    static Address addr_;
};

Address ExternalMappingTest::addr_ = 0;

TEST_F(ExternalMappingTest, get_follows_the_page_table_and_a_host_pointer_does_not) {
    MemState &mem = shared_state();
    const Ptr<Payload> handle(addr_);

    Payload *const in_the_arena = handle.get(mem);
    ASSERT_NE(in_the_arena, nullptr);
    in_the_arena->magic = 0x4E4753; // "NGS"
    in_the_arena->counter = 41;

    // The pointer NGS used to cache. It stays valid as a value and stops being
    // valid as a way to reach the guest data.
    Payload *const cached_host_pointer = in_the_arena;

    AlignedBuffer buffer;
    ASSERT_NE(buffer.get(), nullptr);
    add_external_mapping(mem, addr_, PAGES * PAGE, buffer.get());

    // The guest address resolves to the mapped buffer now, and the data is
    // there. This is what a Ptr member relies on.
    Payload *const after_mapping = handle.get(mem);
    EXPECT_NE(after_mapping, in_the_arena);
    EXPECT_EQ(after_mapping->magic, 0x4E4753);
    EXPECT_EQ(after_mapping->counter, 41);

    // The cached host pointer keeps its value and no longer addresses the guest
    // data. The test does not read through it: its target was protected
    // PROT_NONE and madvised away, so the read would fault. This is the failure
    // the Ptr<> members replaced.
    EXPECT_EQ(cached_host_pointer, in_the_arena);
    EXPECT_NE(cached_host_pointer, after_mapping);

    remove_external_mapping(mem, buffer.get(), PAGES * PAGE);

    // The data came back with the arena and the pointer is the arena again.
    EXPECT_EQ(handle.get(mem), in_the_arena);
    EXPECT_EQ(in_the_arena->magic, 0x4E4753);
    EXPECT_EQ(in_the_arena->counter, 41);
}

TEST_F(ExternalMappingTest, get_is_the_identity_while_no_mapping_covers_the_address) {
    MemState &mem = shared_state();
    const Ptr<Payload> handle(addr_);
    Payload *const first = handle.get(mem);
    EXPECT_EQ(handle.get(mem), first);
    EXPECT_EQ(reinterpret_cast<uint8_t *>(first), mem.memory.get() + addr_);
}

TEST_F(ExternalMappingTest, get_of_a_null_address_is_null) {
    EXPECT_EQ(Ptr<Payload>(0).get(shared_state()), nullptr);
}

TEST_F(ExternalMappingTest, get_does_nothing_without_a_page_table) {
    // The stock Adreno path runs DoubleBuffer, where add_external_mapping
    // returns at once and a host pointer can never go stale.
    //
    // This state is static for the reason given above: init() installs a
    // process-wide handler that closes over it, and a local would leave that
    // handler pointing at a frame that has already returned.
    static MemState linear;
    ASSERT_TRUE(init(linear, false));

    const Address addr = alloc(linear, PAGE, "external mapping linear test");
    ASSERT_NE(addr, 0);
    EXPECT_EQ(Ptr<uint8_t>(addr).get(linear), linear.memory.get() + addr);

    deinit_mem(linear);
}

} // namespace