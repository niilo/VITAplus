# 07: sceKernelGetThreadmgrUIDClass returns UNIMPLEMENTED

Status: open
Type: task
Label: ready-for-agent

## Context

Two exports answer `UNIMPLEMENTED()`:

- `sceKernelGetThreadmgrUIDClass`,
  `vita3k/modules/SceKernelThreadMgr/SceThreadmgr.cpp:1237`
- `ksceKernelGetThreadmgrUIDClass`,
  `vita3k/modules/SceKernelThreadMgr/SceThreadmgrForDriver.cpp:188`

On the Vita, given a UID the call returns the class number of the object that
UID names, or `SCE_KERNEL_ERROR_UNKNOWN_UID` when nothing owns it:

| Class | Object |
| --- | --- |
| 1 | thread |
| 2 | semaphore |
| 3 | event flag |
| 4 | mutex |
| 5 | condvar |
| 6 | timer |
| 7 | msg pipe |
| 8 | callback |
| 9 | thread event |
| 10 | lw mutex |
| 11 | lw cond |
| 12 | rw lock |
| 13 | simple event |

Games use it to check that a handle they were given is the kind they expect,
before using it. Returning `UNIMPLEMENTED` instead of the class number makes
that check fail, and a title that treats the failure as fatal stops.

Every map the answer needs already exists on `KernelState`, all under
`kernel.mutex`: `threads`, `semaphores`, `eventflags`, `mutexes`, `lwmutexes`,
`condvars`, `lwcondvars`, `timers`, `msgpipes`, `callbacks`, `rwlocks` and
`simple_events`.

EmuCoreV commit `ffc2cebb` implements it in a new
`kernel/include/kernel/uid_class.h` and `kernel/src/uid_class.cpp`.

## Plan

1. Add `kernel/include/kernel/uid_class.h` and `kernel/src/uid_class.cpp` with
   one function that takes `KernelState &` and a `SceUID` and returns the
   class number or `SCE_KERNEL_ERROR_UNKNOWN_UID`. Take `kernel.mutex` once
   and read all the maps under it, rather than calling `get_thread` and the
   other accessors in turn.
2. Wire both exports to it. `SceThreadmgrForDriver.cpp` has no `emuenv` in
   scope in the usual way; check how the other exports in that file reach the
   kernel state before writing the call.
3. Add the two files to `vita3k/kernel/CMakeLists.txt`.
4. Add a test that creates one object of each class, asks for its class, and
   asks for a UID that was never allocated.

## Comments