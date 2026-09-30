# 03: DARIUSBURST Chronicle Saviours aborts in std::terminate 1.5 seconds after the boot

Status: open
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
