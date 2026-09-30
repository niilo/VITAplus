# 01: Jak and Daxter Collection crashes in an intro video (sceClibVsnprintf, bad pointer)

Status: resolved
Type: task
Label: ready-for-agent

## Symptom

Jak and Daxter Collection (PCSA00080) on the Ayaneo Pocket S (Adreno 740),
build `4359f6de`, release APK, org.vita3k.emulator. The game crashes about
2 seconds after the boot, while it starts an intro video. Found in the game
sweep of 2026-09-30.

## Evidence

Last lines of `vita3k.log`:

```
D/ SceAvPlayerAutoPlayEventCallback [123]:  Received Event SCE_AVPLAYER_STATE_READY
D/ SceAvPlayerAutoPlayEventCallback [153]: Audio on Stream 0 - Channels 2, Sampling Rate 48000, Language eng
[CRASH] Refused read to INVALID guest address 0x3FF00000 (host 0x43FF00000)
  guest PC=0x8118A594 LR=0x8107162D last_import=0xFA6BE467 (sceClibVsnprintf)
```

The guest calls `sceClibVsnprintf`, and the call reads from the guest address
`0x3FF00000`. That value is the upper half of the double `1.0`
(`0x3FF0000000000000`). The guest most likely formats a video stream line
(the game prints it after the audio stream line) and takes a `%s` or `%f`
argument from the wrong place in the variable argument list. Then the emulator
aborts (signal 6) after the access violation.

## Next steps

1. Read the `sceClibVsnprintf` and `sceClibPrintf` code in
   `vita3k/modules/SceLibc*` or `SceClib*`. Check how they read `%f`, `%lf`
   and 64-bit arguments from the guest variable argument list on ARM32
   (doubles are 8-byte aligned and can skip a register).
2. Find the format string of the video line (guest PC `0x8118A594`,
   LR `0x8107162D`) and check the arguments against the values that the
   `SceAvPlayer` HLE returns for the video stream (width, height, aspect
   ratio, duration).
3. Fix the argument reading or the stream data. Test with PCSA00080 on the
   device.
4. Check the same game on Vita3K-Plus (`org.vita3kplus.emulator`) to see
   if this is a regression.

## Comments

2026-09-30: Fixed. The va_list path of `module::vargs::next`
(`vita3k/module/include/module/vargs.h`) did not align 8-byte values to
8 bytes. The ARM ABI requires it. The guest line prints the aspect ratio as a
`%f` double (`1.0`), so the next `%s` read the wrong word. After the fix, the
line prints "Video on Stream 1 - Width 960 Height 544 Aspect Ratio 1.000000"
and the game no longer crashes (release APK, device test, 60 seconds). The
game then shows a black screen: see issue 05.
