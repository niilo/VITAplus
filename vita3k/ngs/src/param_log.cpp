// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
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

#include <ngs/param_log.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

namespace ngs {

bool is_new_param_set(const uint32_t module_id, const void *params, const size_t size) {
    static constexpr size_t max_sets = 16;
    static std::mutex mutex;
    static std::map<uint32_t, std::vector<std::vector<uint8_t>>> seen;

    const std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::vector<uint8_t>> &sets = seen[module_id];
    if (sets.size() >= max_sets)
        return false;

    const uint8_t *bytes = static_cast<const uint8_t *>(params);
    std::vector<uint8_t> set(bytes, bytes + size);
    if (std::find(sets.begin(), sets.end(), set) != sets.end())
        return false;

    sets.push_back(std::move(set));
    return true;
}

} // namespace ngs
