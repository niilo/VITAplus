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

#include <util/perf_log.h>

#include <util/log.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <map>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace perf_log {

namespace {

struct Channel {
    std::ofstream file;
    std::vector<std::string> pending;
};

struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::map<std::string, Channel, std::less<>> channels;
    fs::path dir;
    std::thread writer;
    bool stopping = false;

    ~State() {
        stop();
    }

    // Called with `mutex` held.
    void flush_locked() {
        for (auto &[name, channel] : channels) {
            for (const auto &line : channel.pending)
                channel.file << line << '\n';
            channel.pending.clear();
            channel.file.flush();
        }
    }

    void run() {
        std::unique_lock lock(mutex);
        while (!stopping) {
            wake.wait_for(lock, std::chrono::seconds(1));
            flush_locked();
        }
    }

    void stop() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wake.notify_all();
        if (writer.joinable())
            writer.join();
        std::lock_guard lock(mutex);
        flush_locked();
        channels.clear();
    }
};

std::atomic_bool s_enabled = false;

State &state() {
    static State s;
    return s;
}

} // namespace

void start(const fs::path &dir) {
    State &s = state();
    // stop() early-returns while s_enabled is false, so a second start would
    // leave the first writer thread running. Clear the state directly.
    s.stop();
    std::lock_guard lock(s.mutex);
    fs::create_directories(dir);
    s.dir = dir;
    s.stopping = false;
    try {
        s.writer = std::thread([&s] { s.run(); });
    } catch (const std::system_error &e) {
        s.stopping = true;
        LOG_ERROR("Could not start the performance log writer thread: {}", e.what());
        return;
    }
    s_enabled = true;
    LOG_INFO("Performance log is on. Folder: {}", dir.generic_string());
}

void stop() {
    // Nothing to do when the log was never on. Reaching state() would build
    // the State object, and its destructor would run at process exit.
    if (!s_enabled)
        return;
    s_enabled = false;
    state().stop();
}

bool enabled() {
    return s_enabled;
}

int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void write(std::string_view channel, std::string_view header, std::string line) {
    if (!s_enabled)
        return;
    State &s = state();
    std::lock_guard lock(s.mutex);
    if (s.stopping)
        return;
    auto it = s.channels.find(channel);
    if (it == s.channels.end()) {
        it = s.channels.emplace(std::string(channel), Channel{}).first;
        const fs::path path = s.dir / (std::string(channel) + ".csv");
        it->second.file.open(path.native(), std::ios::out | std::ios::trunc);
        if (!it->second.file)
            LOG_ERROR("Could not open {}", path.generic_string());
        it->second.file << header << '\n';
    }
    it->second.pending.push_back(std::move(line));
}

} // namespace perf_log
