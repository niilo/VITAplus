# 05: Jak and Daxter Collection stays on a black screen after the intro video starts

Status: open
Type: task
Label: ready-for-agent

## Symptom

Jak and Daxter Collection (PCSA00080) on the Ayaneo Pocket S (Adreno 740),
release APK with the fix of issue 01. The game no longer crashes, but the
screen stays black (the FPS counter shows 30) at 60 seconds after the start.
Found on 2026-09-30.

## Evidence

- The watchdog reports, at 12 seconds: "STUCK-SCENE WATCHDOG: presenting (358
  SetFrameBuf accepted) but only 2 pipeline(s) ever compiled, frozen for 720
  vblanks (~12s) - wedged BEFORE scene render".
- The main thread (`PCSA00080`, thread 8) is running at `pc 0x81189424`, the
  import stub for NID `0x8BD94593`.
- Threads `SndStreamThread` (78, 81), `avPlayer FileStreaming` (105) and
  `avPlayer VideoDec` (194) wait. `WAIT_BLOCK` and `WAIT_DONE` lines repeat
  with result `0x80028005` (timeout) for threads 193, 194 and 81.
- One error before the wait: `_sceKernelLockLwMutex returned
  SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID (0x80028181)`, 1 ms after
  "Video on Stream 1 - Width 960 Height 544".
- "Game is using unimplemented distortion audio module" (open issue
  `log-review-2026-09-29/06`).
- The log of this run: pull with `tools/android/device.sh log
  org.vita3k.emulator <dir>`. The session starts at the last "Game started"
  line.

## Next steps

1. Find out what NID `0x8BD94593` is (`vita3k/nids/include/nids/nids.inc`)
   and what the main thread waits for.
2. Check the `SceAvPlayer` HLE: the game uses its own player library
   (`simple_mp4_player`) on top of the avplayer imports. Look at which
   avplayer calls the game makes after the "READY" event and which one does
   not return the expected data (frame, state change).
3. Find the source of the invalid lw mutex ID.
4. Compare with Vita3K-Plus (`org.vita3kplus.emulator`).

## Comments
