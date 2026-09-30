# 04: Sky Force Anniversary aborts while it loads level0 (JIT fault on a protected page)

Status: resolved
Type: task
Label: ready-for-agent

## Symptom

Sky Force Anniversary (PCSE00865, a Unity game with Mono) on the Ayaneo
Pocket S (Adreno 740), build `4359f6de`, release APK, org.vita3k.emulator.
The game runs for 18 seconds and then aborts while it loads `level0`. Found in
the game sweep of 2026-09-30.

## Evidence

Last lines of `vita3k.log`:

```
[17:01:14] TTY: entering exception wait...
[17:01:14] Stubbed sceKernelWaitExceptionForMono import called. (Blocks forever (no exception delivery))
...
Opening file: app0:/Media/sharedassets0.assets flags=0x1 -> fd 109
Missing file .../PCSE00865/Media/level0.resG
Missing file .../PCSE00865/Media/level0.res
Missing file .../PCSE00865/Media/level0.resS
Opening file: app0:/Media/level0 flags=0x1 -> fd 110
[CRITICAL] std::terminate called without an active exception
[CRASH] fatal signal 6 ... libVita3K.so+0x15D8CB8
```

- The log has about 170 `stat_file` errors and 77 `open_file` errors
  (`0x80010002`, file not found). Most are optional Unity files (mono
  `policy.2.0`, `unity default resources`, `.resS`). The Unity engine
  probes for them, so they are probably normal.
- `sceKernelWaitExceptionForMono` is a stub that blocks forever.
- `std::terminate` was called directly (no active exception). The handler
  is in `vita3k/app/src/app_init.cpp:420`. It prints no stack.
- The same abort code address (`libVita3K.so+0x15D8CB8`) appears in the
  Jak and Daxter crash, so it is the abort path only, not the cause.

## Next steps

1. Get a native stack (see issue 03, same method).
2. Check whether the level0 load reads a file that really is missing on
   this install: compare `Media/` with a known good dump of PCSE00865.
3. Decide if `sceKernelWaitExceptionForMono` needs real exception delivery.
4. Check the same game on Vita3K-Plus (`org.vita3kplus.emulator`) to see
   if this is a regression.

## Comments

2026-09-30: Fixed. The cause was not the missing files or
`sceKernelWaitExceptionForMono`.

`stdio.log` (see issue 03) showed two faults:

1. "Unhandled SIGSEGV at pc ..." in host code. Symbolicated, it is
   `VKSurfaceCache::perform_post_surface_sync` ->
   `swizzle_text_T` (`vita3k/renderer/src/vulkan/surface_cache.cpp:2719`).
   The renderer thread writes surface data into guest memory that is
   protected. Our handler recovers from this.
2. "dynarmic: Segfault happened within JITted code ... wasn't at a fastmem
   patch location". The JIT code stored (`str w21, [x16, x19]`) to an
   external mapping (fault address `0x6D34CA0000`, SEGV_ACCERR) that the
   emulator had write protected. Our handler can recover from this, but
   Dynarmic's handler runs before it, because Dynarmic installs its SIGSEGV
   handler with the first JIT, after ours. It finds the JIT block, finds no
   fastmem patch entry, and calls `std::terminate`.

Fix: `prioritize_fault_handler` (`vita3k/mem/src/mem.cpp`, called from the
`DynarmicCPU` constructor after the first JIT is made, in page-table mode only)
installs our handler again, so that it runs first. What it cannot handle goes
to Dynarmic's handler, which is saved. If Dynarmic's handler recovered (the
program counter changed), the signal handler returns. Otherwise the crash log
is written as before. The log line "The SIGSEGV handler runs before the
handler of Dynarmic" shows that it is active.

Device test (release APK): the game reaches its main menu at 56 FPS. A dialog
"An error occurred. Error code: 0x0" shows (probably the PSN sign-in). The
dialog is not a crash.

