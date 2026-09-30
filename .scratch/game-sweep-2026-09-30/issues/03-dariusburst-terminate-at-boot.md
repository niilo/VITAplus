# 03: DARIUSBURST Chronicle Saviours aborts 1.5 seconds after the boot (JIT fault on an unmapped page)

Status: resolved
Type: task
Label: ready-for-agent

## Symptom

DARIUSBURST Chronicle Saviours (PCSE00792) on the Ayaneo Pocket S
(Adreno 740), build `4359f6de`, release APK, org.vita3k.emulator. The game
aborts right after its first file reads. Found in the game sweep of
2026-09-30.

## Evidence

Last lines of `vita3k.log`:

```
Game started: DARIUSBURST Chronicle Saviours (PCSE00792)
Unimplemented sceGxmIsDebugVersion import called.
Opening file: app0:package_region.dat flags=0x1 -> fd 3
Opening file: app0:jp/data/mediapaths/mediapaths.dxt flags=0x1 -> fd 4
[CRITICAL] std::terminate called without an active exception
```

Logcat shows signal 6 (Aborted). The message means that `std::terminate` was
called directly, not by an escaped exception. The handler is in
`vita3k/app/src/app_init.cpp:420`. It prints no thread name and no stack.
The log has no `[CRASH] frame` lines for this abort.

Possible direct callers of `std::terminate`: a `std::thread` that is
destroyed while joinable, a `noexcept` function that throws (this case would
have an active exception), and an explicit call.

## Next steps

1. Get a native stack. Use a build with symbols, or make the terminate
   handler print the backtrace that the signal handler already prints for
   signals (use the same code).
2. Find the caller. Look at what runs after the read of
   `app0:jp/data/mediapaths/mediapaths.dxt` (the read starts a guest thread
   or a decoder).
3. Fix it and test PCSE00792 on the device.
4. Check the same game on Vita3K-Plus (`org.vita3kplus.emulator`) to see
   if this is a regression.

## Comments

2026-09-30: The abort is fixed. Steps that found the cause:

1. The improved crash handler (issue 02) gave a chain of frames. Symbolicated,
   it is `Dynarmic::Backend::Arm64::AddressSpace::FastmemCallback`
   (`external/dynarmic/src/dynarmic/backend/arm64/address_space.cpp:347`). The
   code prints "Segfault wasn't at a fastmem patch location!" and calls
   `ASSERT_FALSE`, which calls `std::terminate`.
2. Dynarmic prints to stdout, which Android discards. The app now redirects
   stdout and stderr to `stdio.log` next to `vita3k.log`
   (`vita3k/app/src/app_init.cpp`). The terminate handler flushes both.
3. With the fault address printed (temporary patch, removed): a JIT load
   `ldr w22, [x16, x20]` read the guest NULL page (`host 0x400000000`), and in
   a second step a wild guest address (`0x10E33AF2`). Both pages are not
   mapped, but their page table entries pointed at the PROT_NONE arena, so
   the load faulted inside JIT code.
4. Only `memory-mapping: page-table` has this problem. The modes
   double-buffer and external-host do not crash. On Windows
   `emulate_refused_jit_access` handles the fault. There is no such code for
   arm64 Linux and Android, and Dynarmic's handler runs first and terminates.

Fix: `MemState` has a second table, `jit_page_table`
(`vita3k/mem/include/mem/state.h`). It equals `page_table`, except that the
entry of a page that is not allocated is null. `alloc_inner`, the remnant code
of `alloc_aligned`, `free` (not when `preserve_freed_pages` is set) and the
external mapping functions keep it up to date. `DynarmicCPU::make_jit` passes
it to Dynarmic. A null entry makes JIT code call `MemoryRead` and
`MemoryWrite` in `dynarmic_cpu.cpp`, which log and refuse the access ("Invalid
read of uint32_t at address: 0x0"). Host code still uses `page_table`, so its
behavior is unchanged.

After the fix the game no longer aborts: it survives the NULL reads
(`0x0`, `0x4`, `0x68`). It then stops for another reason. See issue 06.

