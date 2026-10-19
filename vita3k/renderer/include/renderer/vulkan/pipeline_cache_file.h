// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace renderer::vulkan {

inline constexpr uint32_t pipeline_cache_magic = 0xBEEF4321;

struct PipelineCacheIdentity {
    uint32_t vendor;
    uint32_t device;
    std::array<uint8_t, 16> uuid;
};

struct PipelineCacheFileView {
    std::span<const uint8_t> hashes;
    std::span<const uint8_t> data;

    uint64_t hash(size_t index) const {
        uint64_t value;
        std::memcpy(&value, hashes.data() + index * sizeof(value), sizeof(value));
        return value;
    }
};

// The existing file stores a native-sized count followed by uint64_t keys and
// the Vulkan driver blob. Check every length before using it as an offset.
inline std::optional<PipelineCacheFileView> read_pipeline_cache_file(std::span<const uint8_t> file, const PipelineCacheIdentity &identity) {
    constexpr size_t prefix_size = sizeof(uint32_t) + sizeof(size_t);
    constexpr size_t driver_header_size = 4 * sizeof(uint32_t) + 16;
    if (file.size() < prefix_size)
        return std::nullopt;

    uint32_t magic;
    size_t count;
    std::memcpy(&magic, file.data(), sizeof(magic));
    std::memcpy(&count, file.data() + sizeof(magic), sizeof(count));
    if (magic != pipeline_cache_magic || count > (file.size() - prefix_size) / sizeof(uint64_t))
        return std::nullopt;

    const size_t hashes_size = count * sizeof(uint64_t);
    const auto data = file.subspan(prefix_size + hashes_size);
    if (data.size() < driver_header_size)
        return std::nullopt;

    std::array<uint32_t, 4> header;
    std::memcpy(header.data(), data.data(), sizeof(header));
    if (header[0] < driver_header_size || header[0] > data.size() || header[1] != 1
        || header[2] != identity.vendor || header[3] != identity.device
        || std::memcmp(data.data() + sizeof(header), identity.uuid.data(), identity.uuid.size()) != 0)
        return std::nullopt;

    return PipelineCacheFileView{ file.subspan(prefix_size, hashes_size), data };
}

} // namespace renderer::vulkan
