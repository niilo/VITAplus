// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <threads/queue.h>
#include <util/gpu_timing.h>

#include <gtest/gtest.h>

#include <future>
#include <limits>
#include <thread>

using namespace std::chrono_literals;

TEST(RendererQueue, PendingDisplayInterruptsWithoutConsumingCommand) {
    Queue<int> queue;
    queue.push(7);
    EXPECT_FALSE(queue.top_until(std::chrono::steady_clock::now() + 1s, [] { return true; }));
    ASSERT_TRUE(queue.top(1));
    EXPECT_EQ(*queue.pop(1), 7);
}

TEST(RendererQueue, CommandArrivalWakesWaiter) {
    Queue<int> queue;
    std::promise<void> checked;
    auto ready = checked.get_future();
    auto result = std::async(std::launch::async, [&] {
        bool first = true;
        return queue.top_until(std::chrono::steady_clock::now() + 5s, [&] {
            if (first) {
                first = false;
                checked.set_value();
            }
            return false;
        });
    });
    ready.wait();
    queue.push(42);
    ASSERT_EQ(result.wait_for(1s), std::future_status::ready);
    const auto item = result.get();
    ASSERT_TRUE(item);
    EXPECT_EQ(*item, 42);
    EXPECT_EQ(*queue.pop(1), 42);
}

TEST(RendererQueue, DisplayPublicationCannotLoseWakeup) {
    // Publish after the predicate has read false but before wait releases the
    // mutex. wake() must synchronize with that release, not lose the event.
    Queue<int> queue;
    std::atomic<bool> display{ false };
    std::promise<void> checked;
    std::promise<void> published;
    auto ready = checked.get_future();
    auto publication = published.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        bool first = true;
        return queue.top_until(std::chrono::steady_clock::now() + 5s, [&] {
            const bool value = display.load();
            if (first) {
                first = false;
                checked.set_value();
                publication.wait();
            }
            return value;
        });
    });
    ready.wait();
    display = true;
    published.set_value();
    queue.wake();
    ASSERT_EQ(waiter.wait_for(1s), std::future_status::ready);
    EXPECT_FALSE(waiter.get());
}

TEST(RendererQueue, AbortWakesWaiterAndRejectsPush) {
    Queue<int> queue;
    std::promise<void> checked;
    auto ready = checked.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        bool first = true;
        return queue.top_until(std::chrono::steady_clock::now() + 5s, [&] {
            if (first) {
                first = false;
                checked.set_value();
            }
            return false;
        });
    });
    ready.wait();
    queue.abort();
    ASSERT_EQ(waiter.wait_for(1s), std::future_status::ready);
    EXPECT_FALSE(waiter.get());
    EXPECT_FALSE(queue.push(7));
}

TEST(RendererQueue, ExpiredDeadlineDoesNotBlock) {
    Queue<int> queue;
    EXPECT_FALSE(queue.top_until(std::chrono::steady_clock::now() - 1s, [] { return false; }));
}

TEST(GpuTiming, ConvertsNanosecondsToMicroseconds) {
    ASSERT_TRUE(gpu_timing::elapsed_us(100, 500, 64, 2.5));
    EXPECT_DOUBLE_EQ(*gpu_timing::elapsed_us(100, 500, 64, 2.5), 1.0);
}

TEST(GpuTiming, HandlesCounterWrapAndUnusedHighBits) {
    EXPECT_DOUBLE_EQ(*gpu_timing::elapsed_us(0xfffffa, 4, 24, 1000), 10);
    EXPECT_DOUBLE_EQ(*gpu_timing::elapsed_us(0xaffffffa, 0xb0000004, 24, 1000), 10);
    EXPECT_DOUBLE_EQ(*gpu_timing::elapsed_us(~uint64_t{ 0 } - 5, 4, 64, 1000), 10);
}

TEST(GpuTiming, UnsupportedTimingDoesNotBecomeZeroTime) {
    EXPECT_FALSE(gpu_timing::elapsed_us(0, 1, 0, 1));
    EXPECT_FALSE(gpu_timing::elapsed_us(0, 1, 65, 1));
    EXPECT_FALSE(gpu_timing::elapsed_us(0, 1, 64, 0));
    EXPECT_FALSE(gpu_timing::elapsed_us(0, 1, 64, -1));
    EXPECT_FALSE(gpu_timing::elapsed_us(0, 1, 64, std::numeric_limits<double>::quiet_NaN()));
}
