# 08: Set the device log level back to info

Status: resolved
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Context

The log on the Ayaneo Pocket S starts with `log-level: trace`. 14932 of the
19770 lines are trace lines (for example each shader instruction). Writing
them costs CPU time during play.

## Plan

1. `tools/android/device.sh config-set org.vita3k.emulator log-level 2`.
   The value is a `spdlog` level: 0 is trace, 1 is debug, 2 is info. The
   default in `vita3k/config/include/config/config.h:183` is 0 (trace).
2. Check the next `vita3k.log` for `log-level: info`.

## Answer

`config-set org.vita3k.emulator log-level 2` done. The next `vita3k.log`
(build 4184-63b59770, `master`) starts with `log-level: info`.

## Comments
