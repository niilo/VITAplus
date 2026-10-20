// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace renderer {
inline bool present_guest_surface(bool cpu_dirty, uint64_t cpu_frame, uint64_t rendered_frame, uint64_t current_frame) {
    return (cpu_dirty || cpu_frame > rendered_frame) && current_frame >= rendered_frame && current_frame - rendered_frame >= 2;
}

struct SurfaceSyncRect {
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    bool empty() const {
        return x1 <= x0 || y1 <= y0;
    }
};

inline SurfaceSyncRect intersect_surface_rects(const SurfaceSyncRect &a, const SurfaceSyncRect &b) {
    return { std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1) };
}

inline std::optional<uint64_t> surface_sync_rows_span(uint32_t stride, uint32_t bytes, uint32_t count) {
    if (!count || !bytes || bytes > stride)
        return std::nullopt;
    return static_cast<uint64_t>(count - 1) * stride + bytes;
}

inline bool copy_surface_sync_rows(uint8_t *dst, const uint8_t *src, uint64_t available, uint32_t stride, uint32_t bytes, uint32_t count) {
    const auto span = surface_sync_rows_span(stride, bytes, count);
    if (!span || *span > available || *span > SIZE_MAX)
        return false;
    for (uint32_t row = 0; row < count; row++) {
        const size_t offset = static_cast<size_t>(row) * stride;
        std::memcpy(dst + offset, src + offset, bytes);
    }
    return true;
}
} // namespace renderer
