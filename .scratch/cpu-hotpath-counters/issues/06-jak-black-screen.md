# Jak and Daxter: black screen after the intro

Date: 2026-10-05. Build: `v1.2.1-dev.15` (`c4d2daed`), Pocket S.

## What was reported

Gameplay audio works. The picture is almost entirely black: geometry,
chests and grass appear, but most surfaces are missing their texture.

## What the swapchain change is not responsible for

`swapchain-scale` was the first suspect, because the black appeared in
the same session that first ran at `swapchain-scale: 0.5`. It is not
the cause. The same black screen happens at both settings:

| run | swapchain-scale | extent | picture |
| --- | --- | --- | --- |
| user session 09:49 | 0.5 | 1280x720 | black |
| our session 10:33 | 0.5 | 1280x720 | black |
| our session 10:41 | 1.0 | 2560x1440 | black |

Dead or Alive 5 renders correctly at 0.5, so the scaled swapchain is
sound on its own, and `surface_matches_window_size()` does not loop:
neither log contains a `[SWAPCHAIN] rebuild` line.

## What the logs actually say

The emulator's own watchdog names the failure:

```
STUCK-SCENE WATCHDOG (dump 1/3): presenting (361 SetFrameBuf accepted,
renderer still executing commands) but only 2 pipeline(s) ever compiled,
frozen for 720 vblanks (~12s) - wedged BEFORE scene render
```

The game keeps calling `sceGxmSetFrameBuf`. The emulator keeps
accepting it. But no scene is ever submitted: only the two startup
pipelines are ever compiled, so the cleared black swapchain is all that
is ever presented. That is the black picture, and it is why textures
are missing while a little geometry still shows: what we see is the
tail of the previous frame, not a live render.

The guest is spinning rather than blocked on I/O. Every `SceFios*`
thread is in `status=wait`, and the guest-side semaphore trace is
dominated by failures:

```
WAIT_DONE ... res=0x80028005   (678 times, vs 55 successes)
```

`0x80028005` is `SCE_KERNEL_ERROR_TIMEOUT`. The timeouts begin within
a second of boot, before the first scene.

## This is pre-existing

The same watchdog fires with the same numbers in sessions that predate
the swapchain work (`tmp/play-jak2`, `tmp/play-jak3`: 360/659/959
`SetFrameBuf`, 2 pipelines). The pipeline-memory failures are gone
(0 `ErrorOutOfHostMemory`, down from 42), so the popping should be
fixed, but the wedge is a separate, older problem.

## Candidates, none confirmed

- `sceGxmWaitEvent` is `UNIMPLEMENTED()` in
  `vita3k/modules/SceGxm/SceGxm.cpp:6052`. This is the API a title
  uses to wait on a GPU event, and a game that spins on it would look
  exactly like this. Not yet proven to be the one Jak needs.
- Other unimplemented imports seen this session, all currently benign
  on their face: `sceAppUtilInit`, `ksceKernelSetPermission`,
  `SceQafMgrForDriver_B9770A13`, `SceThreadmgrForDriver_20C228E4`,
  `_sceFiosKernelOverlayThreadSetDisabled`,
  `_sceFiosKernelOverlayGetRecommendedScheduler`,
  `sceNgsSystemSetFlags`.

## Next

1. Instrument the guest semaphore wait to log which syscall returns
   `0x80028005`, and with what handle and timeout. That names the API
   instead of guessing.
2. Implement `sceGxmWaitEvent` and see whether Jak advances. The
   correct behaviour is to block until the referenced event is
   signalled, then return.
3. Only after the wedge is fixed, revisit the texture question: with
   no live render, texture correctness cannot be judged yet.