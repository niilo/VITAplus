# 06: DARIUSBURST Chronicle Saviours: the main thread jumps into data and halts, black screen

Status: open
Type: task
Label: ready-for-agent

## Symptom

DARIUSBURST Chronicle Saviours (PCSE00792) on the Ayaneo Pocket S (Adreno 740),
after the fix of issue 03. The game no longer aborts, but the main thread
stops 1.3 seconds after the boot and the screen stays black. Only 31 frames
are presented.

## Evidence

```
[MemoryRead]: Invalid read of uint32_t at address: 0x0   (mov.w r1, #-1)
[MemoryRead]: Invalid read of uint8_t  at address: 0x0   (adds r4, r0, #0)
[MemoryRead]: Invalid read of uint32_t at address: 0x68  (cmp r0, #0)
[MemoryRead]: Invalid read of uint32_t at address: 0x4   (cmp r0, #0)
open_file: Missing savedata0:data/unregistered_data_0.dat  (0x80010002)
Further invalid guest memory accesses will be suppressed (guest likely spinning on a bad pointer)
[ExceptionRaised]: Halting thread: undefined instruction at address 0x85C79AC4, instruction 0x8167206D
  PC: 0x85c79ac8  LR: 0x80346531  Thumb: false
Thread PCSE00792 (8) experienced a cpu error.
```

- Address `0x85C79AC4` is in the guest heap (not code). The main thread
  ran data, so a function pointer or a return address was wrong. The
  `0x8167206D` value looks like a Thumb code address of the module
  `alevinVita` (`0x81672xxx`), read as an instruction.
- The game reads NULL-based addresses (`0x0`, `0x4`, `0x68`): a NULL object
  pointer. The game may not expect a NULL there. A result from an earlier
  import that returned an error or a stub value is a suspect. Logged before:
  `sceNetInit returned 0x80410110`, `sceNetCtlInit returned
  SCE_NET_CTL_ERROR_NOT_TERMINATED`, `sceGxmPadHeartbeat returned
  SCE_GXM_ERROR_INVALID_POINTER`, the trophy and NP Toolkit (`Toolkit::NP`
  thread) start-up, and `sceGxmIsDebugVersion` (unimplemented).
- The double-buffer and external-host memory modes also end in a black
  screen, so this is not a problem of the page-table mode.
- The other threads wait normally (CRI audio and file system threads,
  `Toolkit::NP`).

## Next steps

1. Find which import returns the NULL object. Log the return values of the
   imports between "Game started" and the first NULL read (the log shows
   `sceKernelGetProcessTimeWide` as the last import of the main thread).
2. Check `sceGxmPadHeartbeat` (`SCE_GXM_ERROR_INVALID_POINTER`) and the
   `sceNet*` results against what the game needs.
3. Check Vita3K-Plus 20588fbf and upstream Vita3K for the same game (the
   game may need a setting or a patch).

## Comments
