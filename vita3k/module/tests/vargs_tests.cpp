// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cpu/functions.h>
#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/state.h>
#include <module/vargs.h>

#include <gtest/gtest.h>

#include <bit>
#include <cstring>

namespace {
MemState &memory() {
    static MemState mem;
    return mem;
}

class VargsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(init(memory(), true));
    }

    void SetUp() override {
        stack = alloc(memory(), 4096, "vargs test");
        ASSERT_NE(stack, 0u);
        cpu = init_cpu(true, 1, 0, memory());
        ASSERT_NE(cpu, nullptr);
        write_sp(*cpu, stack);
    }

    void TearDown() override {
        cpu.reset();
        free(memory(), stack);
    }

    template <typename T>
    void put(size_t offset, T value) {
        std::memcpy(Ptr<uint8_t>(stack + offset).get(memory()), &value, sizeof(value));
    }

    CPUStatePtr cpu;
    Address stack = 0;
};

TEST_F(VargsTest, double_uses_ieee_bits_in_the_aligned_register_pair) {
    constexpr uint64_t values[]{ 0, 0x8000000000000000, 0xc045100000000000, 0x7ff0000000000000, 0x7ff8000000000123 };
    for (const uint64_t bits : values) {
        write_reg(*cpu, 1, 0xdeadbeef);
        write_reg(*cpu, 2, static_cast<uint32_t>(bits));
        write_reg(*cpu, 3, static_cast<uint32_t>(bits >> 32));
        module::vargs args(LayoutArgsState{ 1, 0, 0 });
        EXPECT_EQ(std::bit_cast<uint64_t>(args.next<double>(*cpu, memory())), bits);
    }
}

TEST_F(VargsTest, arguments_after_a_register_double_use_the_stack) {
    const uint64_t bits = std::bit_cast<uint64_t>(-42.125);
    write_reg(*cpu, 2, static_cast<uint32_t>(bits));
    write_reg(*cpu, 3, static_cast<uint32_t>(bits >> 32));
    put<uint32_t>(0, 17);
    put<uint32_t>(4, 0xdeadbeef);
    put<double>(8, 3.5);
    module::vargs args(LayoutArgsState{ 1, 0, 0 });
    EXPECT_DOUBLE_EQ(args.next<double>(*cpu, memory()), -42.125);
    EXPECT_EQ(args.next<uint32_t>(*cpu, memory()), 17u);
    EXPECT_DOUBLE_EQ(args.next<double>(*cpu, memory()), 3.5);
}

TEST_F(VargsTest, guest_va_list_aligns_doubles_and_preserves_following_values) {
    put<uint32_t>(0, 23);
    put<uint32_t>(4, 0xdeadbeef);
    put<double>(8, -0.125);
    put<uint32_t>(16, 29);
    module::vargs args(stack);
    EXPECT_EQ(args.next<uint32_t>(*cpu, memory()), 23u);
    EXPECT_DOUBLE_EQ(args.next<double>(*cpu, memory()), -0.125);
    EXPECT_EQ(args.next<uint32_t>(*cpu, memory()), 29u);
}
} // namespace
