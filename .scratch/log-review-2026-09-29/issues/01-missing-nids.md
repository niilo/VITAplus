# 01: Add HLE stubs for five missing NIDs

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

The log has these lines:

| NID | Thread | Game |
| --- | --- | --- |
| 0xA412E9CA | cNearUtility | Uncharted |
| 0x49A97D5F | cNearUtility | Uncharted |
| 0x69EE6FB3 | cNearUtility | Uncharted |
| 0xE608000B | PCSA00029 | Uncharted |
| 0xD141C076 | PCSA00015 | WipEout 2048 |

`call_import` (`vita3k/modules/module_parent.cpp:172`) returns 0 for a NID
with no function. The games did not fail, but the value 0 is a guess.

Names, from `Princess-of-Sleeping/vita-header-internal` and
`Princess-of-Sleeping/SceKernelModulemgr-Reverse`:

- The first four are in library `SceNearUtil` (NID 0x560D1608), firmware
  1.000 to 3.570. They have no public names. Vita3K's convention for such
  functions is `<Library>_<NID>` (for example `SceThreadmgrForDriver_20C228E4`).
  The first three run right after `libSceNearUtil` loads. 0xE608000B runs
  right before the game unloads it.
- 0xD141C076 is `sceKernelGetCompiledSdkVersionByPidForDriver` in
  `SceProcessmgrForDriver`. The firmware module `vs0:sys/external/libssl.suprx`
  (LLE) calls it after it loads.

## Plan

1. Add `NID(SceNearUtil_A412E9CA, 0xA412E9CA)` and the other three to the
   `SceNearUtil` block of `vita3k/nids/include/nids/nids.inc`. Add
   `EXPORT` stubs that return `UNIMPLEMENTED()` to
   `vita3k/modules/SceNearUtil/SceNearUtil.cpp`. This matches the other
   NearUtil functions, which are all stubs. The log then names the function.
2. Add `NID(ksceKernelGetCompiledSdkVersionByPidForDriver, 0xD141C076)` to
   the `SceProcessmgrForDriver` block. Implement it in
   `vita3k/modules/SceProcessmgr/SceProcessmgrForDriver.cpp`: return the
   SDK version of the main module, the same as
   `sceKernelGetMainModuleSdkVersion`.
3. Implement `_vshKernelGetCompiledSdkVersionByPid`
   (`vita3k/modules/SceVshBridge/SceVshBridge.cpp`) the same way.
4. Build and run the tests in the container.

## Answer

Done on branch `log-review/01-02-nids-visibility`:

- `SceNearUtil_49A97D5F`, `SceNearUtil_69EE6FB3`, `SceNearUtil_A412E9CA`
  and `SceNearUtil_E608000B` are stubs that return 0. The return value is
  the same as before. The log now names them once, as
  `Unimplemented SceNearUtil_... import called`.
- `ksceKernelGetCompiledSdkVersionByPidForDriver` and
  `_vshKernelGetCompiledSdkVersionByPid` take `(pid, SceUInt32 *)`. They
  write the SDK version of the main module and return 0. The signature is
  from `vita-headers/include/psp2/vshbridge.h` and
  `SceKernelModulemgr-Reverse/src/import_defs.h`. Before this change,
  `libssl.suprx` got 0 as the return value and its output variable stayed
  unwritten.

Linux build, `container/vita3k.sh test` and `format-check` pass.

## Comments
