# 13: Vulkan depth-transfer requests leak their fence and command buffer

Status: open
Type: task
Label: ready-for-agent

## Context

`try_transfer_depth_gpu` submits a copy and then hands the objects it must
release to a request queue. `vita3k/renderer/src/vulkan/surface_cache.cpp:1518`:

```cpp
CallbackRequestFunction vk_callback = [&state = this->state, fence, transfer_cmd]() {
    const auto result = state.device.waitForFences(fence, vk::True, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess)
        LOG_ERROR("Could not wait for the depth transfer fence.");

    state.device.destroyFence(fence);

    std::lock_guard<std::mutex> lock(state.multithread_pool_mutex);
    state.device.freeCommandBuffers(state.multithread_command_pool, transfer_cmd);
};
state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(vk_callback)) });
```

The queue's `push` (`vita3k/threads/include/threads/queue.h:80`) returns
without enqueuing when the queue is aborted:

```cpp
while (!aborted && queue_.size() == maxPendingCount_) {
    cond_.wait(mlock);
}
if (aborted) {
    return;
}
```

It reports nothing. The `CallbackRequestFunction` is the only owner of the
fence and the command buffer, so on an aborted queue both are never destroyed.
The reader thread exits when `pop` returns null
(`vita3k/renderer/src/vulkan/context.cpp:91`), and `VKState::cleanup` aborts
the queue at `vita3k/renderer/src/vulkan/renderer.cpp:1249` and resets it at
`:1341`, which drops every pending request without deleting it.

This runs on the Android build. `vita3k/renderer/src/transfer.cpp:45` sets
`use_gpu_depth_transfer = true` with no condition, and `:196` gates only on
`renderer.current_backend == Backend::Vulkan`. So every `transfer_copy` on
every Vulkan backend, stock Qualcomm driver and Turnip alike, takes this path.

The same unowned push appears at `surface_cache.cpp:2118`, `:2183`, `:2190` and
`:2265`.

EmuCoreV commit `dfa7e15a` adds `try_push` that reports rejection, a reader
counter, and a cleanup owner released either by the reader or synchronously on
the publishing thread.

## Plan

1. Add `bool try_push(const T &item)` to
   `vita3k/threads/include/threads/queue.h`, reporting whether the item was
   enqueued. `push` can then call it and drop the result, so no caller has to
   change.
2. Count the active GPU request readers on `VKState`
   (`vita3k/renderer/include/renderer/vulkan/state.h`). A new reader registers
   before it can observe an aborted queue.
3. Wrap the cleanup in an owner type that is released by the reader when it
   consumes the request, and synchronously by the publishing thread when
   `try_push` refuses. The fence has to be waited on before it is destroyed,
   so the synchronous path waits too.
4. Call the cleanup from `VKState::cleanup` before `context = nullptr`
   (`vita3k/renderer/src/vulkan/renderer.cpp:1249`).
5. Apply the same owner to the other four push sites.
6. Skip EmuCoreV's `keep_stopped` argument on the reset path. It exists to
   re-abort after a save-state reset, and this repository has no save states.
7. Note the second path: the wait thread only exists when memory mapping is on
   (`vita3k/renderer/src/vulkan/creation.cpp:56`). With
   `memory-mapping: disabled` nothing consumes the queue, so the current code
   leaks on every call, not only at shutdown. Confirm which behaviour the fix
   gives in that configuration and record it here.

## Comments