// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <shader/spirv_recompiler.h>

#include <SPIRV/spirv.hpp>
#include <gtest/gtest.h>

#include <map>
#include <set>

#include <fstream>

namespace {
shader::usse::SpirvCode compile_fetch(bool raw) {
    // Use the project's own shader and request framebuffer input in its header.
    std::ifstream file(VITA_TEST_SOURCE_DIR "/tools/native-tool/src/shaders/color_f.gxp", std::ios::binary | std::ios::ate);
    if (!file)
        return {};
    const size_t size = file.tellg();
    std::vector<uint32_t> bytes((size + 3) / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(bytes.data()), size);
    auto &program = *reinterpret_cast<SceGxmProgram *>(bytes.data());
    program.program_flags |= SCE_GXM_PROGRAM_FLAG_FRAGCOLOR_USED;
    FeatureState features{};
    features.direct_fragcolor = true;
    features.preserve_f16_nan_as_u16 = true;
    shader::Hints hints{};
    hints.color_format = SCE_GXM_COLOR_FORMAT_F16F16F16F16_RGBA;
    hints.raw_color_attachment_input = raw;
    return shader::convert_gxp(program, "framebuffer-fetch-test", features, shader::Target::SpirVVulkan, hints).spirv;
}

struct FetchReads {
    size_t float_reads = 0;
    size_t uint_reads = 0;
    bool raw_attachment = false;
};

FetchReads inspect_fetch(const shader::usse::SpirvCode &code) {
    std::set<uint32_t> uint_types, float_types;
    std::map<uint32_t, uint32_t> vector_types;
    std::set<uint32_t> attachment_one, binding_two;
    FetchReads result;
    for (size_t offset = 5; offset < code.size();) {
        const uint32_t count = code[offset] >> 16;
        if (!count || count > code.size() - offset)
            break;
        const auto op = static_cast<spv::Op>(code[offset] & 0xFFFF);
        const auto *words = code.data() + offset;
        if (op == spv::OpTypeInt && count == 4 && words[2] == 32 && words[3] == 0)
            uint_types.insert(words[1]);
        if (op == spv::OpTypeFloat && count == 3 && words[2] == 32)
            float_types.insert(words[1]);
        if (op == spv::OpTypeVector && count == 4 && words[3] == 4)
            vector_types[words[1]] = words[2];
        if (op == spv::OpDecorate && count == 4) {
            if (words[2] == spv::DecorationInputAttachmentIndex && words[3] == 1)
                attachment_one.insert(words[1]);
            if (words[2] == spv::DecorationBinding && words[3] == 2)
                binding_two.insert(words[1]);
        }
        if (op == spv::OpImageRead && count >= 5) {
            result.float_reads += float_types.contains(vector_types[words[1]]);
            result.uint_reads += uint_types.contains(vector_types[words[1]]);
        }
        offset += count;
    }
    for (const auto id : attachment_one)
        result.raw_attachment |= binding_two.contains(id);
    return result;
}

TEST(FramebufferFetch, half_float_fetch_reads_the_unsigned_input_attachment) {
    const auto spirv = compile_fetch(true);
    ASSERT_GT(spirv.size(), 5);
    const auto reads = inspect_fetch(spirv);
    EXPECT_TRUE(reads.raw_attachment);
    EXPECT_EQ(reads.uint_reads, 1);
    EXPECT_EQ(reads.float_reads, 1);
}

TEST(FramebufferFetch, ordinary_fetch_keeps_one_float_input_attachment) {
    const auto spirv = compile_fetch(false);
    ASSERT_GT(spirv.size(), 5);
    const auto reads = inspect_fetch(spirv);
    EXPECT_FALSE(reads.raw_attachment);
    EXPECT_EQ(reads.uint_reads, 0);
    EXPECT_EQ(reads.float_reads, 1);
}
} // namespace
