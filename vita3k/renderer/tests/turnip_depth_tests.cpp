// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <renderer/vulkan/turnip.h>

#include <gtest/gtest.h>

namespace {
class TurnipDepth : public testing::Test {
protected:
    renderer::GxmRecordState record{};
    SceGxmProgram program{};

    void SetUp() override {
        program.program_flags = SCE_GXM_PROGRAM_FLAG_FRAGCOLOR_USED;
        record.front_stencil_state_op.func = SCE_GXM_STENCIL_FUNC_ALWAYS;
        record.back_stencil_state_op.func = SCE_GXM_STENCIL_FUNC_ALWAYS;
    }

    bool needs_workaround(bool turnip = true, bool fetch = true) const {
        return renderer::vulkan::fetch_draw_stops_lrz_write(turnip, fetch, record, program);
    }
};

TEST_F(TurnipDepth, framebuffer_read_with_depth_writes_uses_workaround) {
    EXPECT_TRUE(needs_workaround());
    record.two_sided = SCE_GXM_TWO_SIDED_ENABLED;
    EXPECT_TRUE(needs_workaround());
}

TEST_F(TurnipDepth, other_drivers_and_non_feedback_paths_keep_stencil) {
    EXPECT_FALSE(needs_workaround(false));
    EXPECT_FALSE(needs_workaround(true, false));
    program.program_flags = 0;
    EXPECT_FALSE(needs_workaround());
}

TEST_F(TurnipDepth, real_stencil_tests_are_preserved) {
    record.front_stencil_state_op.func = SCE_GXM_STENCIL_FUNC_EQUAL;
    EXPECT_FALSE(needs_workaround());
    record.front_stencil_state_op.func = SCE_GXM_STENCIL_FUNC_ALWAYS;
    record.back_stencil_state_op.func = SCE_GXM_STENCIL_FUNC_EQUAL;
    EXPECT_TRUE(needs_workaround());
    record.two_sided = SCE_GXM_TWO_SIDED_ENABLED;
    EXPECT_FALSE(needs_workaround());
}

TEST_F(TurnipDepth, disabled_fragment_and_depth_write_paths_are_preserved) {
    record.front_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_DISABLED;
    EXPECT_FALSE(needs_workaround());
    record.front_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;
    record.front_depth_write_mode = SCE_GXM_DEPTH_WRITE_DISABLED;
    EXPECT_FALSE(needs_workaround());
    record.front_depth_write_mode = SCE_GXM_DEPTH_WRITE_ENABLED;
    program.program_flags |= SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED;
    EXPECT_FALSE(needs_workaround());
}
} // namespace
