// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace gpu_timing {

// Vulkan timestamps wrap at the queue family's timestampValidBits. The
// interval must be shorter than one counter period to be unambiguous.
inline std::optional<double> elapsed_us(uint64_t start, uint64_t end, uint32_t valid_bits, double period_ns) {
    if (valid_bits == 0 || valid_bits > 64 || !std::isfinite(period_ns) || period_ns <= 0)
        return std::nullopt;
    const uint64_t mask = valid_bits == 64 ? ~uint64_t{ 0 } : (uint64_t{ 1 } << valid_bits) - 1;
    return static_cast<double>((end - start) & mask) * period_ns / 1000.0;
}

} // namespace gpu_timing
