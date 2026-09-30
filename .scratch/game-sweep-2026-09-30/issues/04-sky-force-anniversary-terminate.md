# 04: Sky Force Anniversary aborts in std::terminate while it loads level0

Status: open
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
