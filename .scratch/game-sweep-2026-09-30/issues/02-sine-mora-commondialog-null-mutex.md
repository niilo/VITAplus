# 02: Sine Mora crashes at boot with a null mutex lock after sceCommonDialogSetConfigParam

Status: open
Type: task
Label: ready-for-agent

## Symptom

Sine Mora (PCSE00090) on the Ayaneo Pocket S (Adreno 740), build `4359f6de`,
release APK, org.vita3k.emulator. The game crashes 1.6 seconds after the boot.
Found in the game sweep of 2026-09-30.

## Evidence

Last lines of `vita3k.log`:

```
Stubbed sceNpTrophyCreateHandle import called. (Stubbed handle with 1)
sceAppUtilSystemParamGetInt(LANG) -> 1
Unimplemented sceCommonDialogSetConfigParam import called.
[CRASH] fatal signal 11 (si_code 1) at address 0x0
  pc = libc.so+0xBB5E0 (pthread_mutex_lock)   lr = unmapped
```

The crash is on the host side: `pthread_mutex_lock` runs on a null pointer.
The last import before it is `sceCommonDialogSetConfigParam`, which is a stub
(`vita3k/modules/SceCommonDialog/SceCommonDialog.cpp:116`). The trophy calls
before it are stubs too (`sceNpTrophyCreateHandle` returns a fixed handle 1).

No native stack frames are in the log (the crash handler did not print
frames for this signal).

## Next steps

1. Get a native stack. Use a debug or reldebug build with symbols, or add
   frames to the crash handler output for this signal, and run PCSE00090.
2. Find which code locks a null mutex. Suspects: the trophy context that
   goes with the stub handle 1 (`SceNpTrophy`), and the common dialog state
   that the game sets up before the first dialog.
3. Fix the null access. If the cause is the stub, implement the missing
   part.
4. Check the same game on Vita3K-Plus (`org.vita3kplus.emulator`) to see
   if this is a regression.

## Comments
