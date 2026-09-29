# 01: Make the plus-master branch

Status: claimed
Claimed: 2026-09-29 Claude Code session (Opus 5.5)
Type: task
Label: ready-for-agent

## Steps

1. Branch `plus-master` from `plus/all-enhancements` 20588fbf.
2. Generate `vita3k/util/include/util/fork_build.h` in the build, because
   Plus does not commit it.
3. Add the tools and docs group from `master`.
4. Build Linux and the Android release APK. Run the tests and the format
   check. Boot Uncharted on the device.

## Comments
