// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace renderer::vulkan {

struct VertexStreamLayout {
    static constexpr uint32_t MAX_ATTRIBUTES = 32;
    struct Copy {
        uint32_t src;
        uint32_t dst;
        uint32_t size;
    };
    bool repack = false;
    uint32_t stride = 0;
    uint32_t record_size = 0;
    std::array<uint32_t, MAX_ATTRIBUTES> offsets{};
    std::array<Copy, MAX_ATTRIBUTES> copies{};
    uint32_t copy_count = 0;
};

struct VertexStreamAttribute {
    uint32_t index;
    uint32_t offset;
    uint32_t size;
    uint32_t alignment;
};

inline VertexStreamLayout make_vertex_stream_layout(uint32_t stride, std::span<const VertexStreamAttribute> attributes, bool require_aligned_stride = false) {
    VertexStreamLayout layout;
    layout.stride = stride;
    bool unaligned = require_aligned_stride && stride % 4 != 0;
    for (const auto &attribute : attributes) {
        layout.offsets[attribute.index] = attribute.offset;
        unaligned |= attribute.offset % attribute.alignment != 0 || stride % attribute.alignment != 0;
    }
    if (!unaligned)
        return layout;

    std::array<VertexStreamAttribute, VertexStreamLayout::MAX_ATTRIBUTES> items{};
    std::copy(attributes.begin(), attributes.end(), items.begin());
    std::sort(items.begin(), items.begin() + attributes.size(), [](const auto &left, const auto &right) {
        return left.offset != right.offset ? left.offset < right.offset : left.size > right.size;
    });
    VertexStreamLayout::Copy block{};
    uint32_t delta = 0;
    for (size_t k = 0; k < attributes.size(); k++) {
        const auto &item = items[k];
        if (k > 0 && (item.offset + delta) % item.alignment == 0) {
            block.size = std::max(block.size, item.offset + item.size - block.src);
        } else {
            if (k > 0)
                layout.copies[layout.copy_count++] = block;
            const uint32_t position = std::max(layout.record_size, item.offset);
            const uint32_t dst = (position + item.alignment - 1) / item.alignment * item.alignment;
            delta = dst - item.offset;
            block = { item.offset, dst, item.size };
        }
        layout.offsets[item.index] = item.offset + delta;
        layout.record_size = std::max(layout.record_size, block.dst + block.size);
    }
    if (!attributes.empty())
        layout.copies[layout.copy_count++] = block;
    layout.stride = stride == 0 ? 0 : (std::max(layout.record_size, stride) + 3) / 4 * 4;
    layout.repack = true;
    return layout;
}

inline size_t repacked_vertex_stream_size(size_t size, uint32_t stride, const VertexStreamLayout &layout) {
    return stride ? ((size + stride - 1) / stride) * layout.stride : layout.record_size;
}

// A final attribute can end before the source stride. Never read that padding.
inline void repack_vertex_stream(std::span<uint8_t> destination, std::span<const uint8_t> source, uint32_t stride, const VertexStreamLayout &layout) {
    std::fill(destination.begin(), destination.end(), 0);
    const size_t count = stride ? (source.size() + stride - 1) / stride : 1;
    for (size_t vertex = 0; vertex < count; vertex++) {
        for (uint32_t c = 0; c < layout.copy_count; c++) {
            const auto &copy = layout.copies[c];
            const size_t src = stride * vertex + copy.src;
            const size_t dst = layout.stride * vertex + copy.dst;
            if (src < source.size() && dst < destination.size())
                memcpy(destination.data() + dst, source.data() + src,
                    std::min({ static_cast<size_t>(copy.size), source.size() - src, destination.size() - dst }));
        }
    }
}

} // namespace renderer::vulkan
