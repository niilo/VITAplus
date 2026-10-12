# 05: sceKernelOpenSimpleEvent is unimplemented and has the wrong signature

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp:1308`:

```cpp
EXPORT(int, sceKernelOpenSimpleEvent) {
    TRACY_FUNC(sceKernelOpenSimpleEvent);
    return UNIMPLEMENTED();
}
```

Two problems. The call is unimplemented, so a title that opens a simple event
by name gets a failure and cannot synchronise on it. And the return type is
`int` where the function returns a `SceUID`, so even the signature is wrong.

The lookup it needs already exists in the same form.
`semaphore_find` (`vita3k/kernel/src/sync_primitives.cpp:1204`) is a name
lookup over a UID-keyed map under `kernel.mutex`. `simple_events` is already a
member of `KernelState`, and `simple_event_delete` already looks objects up in
it.

`vita3k/kernel/include/modules/library_init_list.inc` needs no change. The
simple event machinery is registered by the sync primitive module.

Vita3K-Plus commit `987bfea4`. The same commit also carries the
`simple_event_delete` fix, which is ticket 05 of `.scratch/emucorev-fixes/`.
Take both from one commit.

## Plan

1. Add `simple_event_find` to
   `vita3k/kernel/include/kernel/sync_primitives.h` and
   `vita3k/kernel/src/sync_primitives.cpp`, modelled on `semaphore_find` at
   `:1204`, taking the name and returning the `SceUID` or
   `SCE_KERNEL_ERROR_INVALID_NAME`.
2. Implement `sceKernelOpenSimpleEvent` with the correct return type. Check the
   NID table under `vita3k/modules/SceKernelThreadMgr/` for the real prototype
   before changing it; the file uses a macro and the argument list has to match
   what the loader supplies.
3. Test: create a simple event with a name, open it, and check that the handle
   matches. Then check the failure path with a name that was never created.

## Comments