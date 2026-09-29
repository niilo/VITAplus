# 11: Uncharted colors are duller than on Vita3K-Plus

Status: open
Type: task
Label: ready-for-agent

## Context

On the Uncharted title screen, the greens of the jungle are brighter and
more saturated on Vita3K-Plus 20588fbf than on our `master`. The Plus
bisect of issue 10 showed where the change is: Plus commit 39 (abe2d71c)
has dull greens, and commit 40 (8be36fa1) has bright greens. A build of
commit 39 with only the `vita3k/shader` and `vita3k/modules/SceGxm` files
of commit 40 has bright greens. So the change is in those files.

Candidates in 8be36fa1: the front-facing global register (g16) in
`usse_utilities.cpp`, the `uniform_block.h` fields, the
`spirv_recompiler.cpp` changes, and the SceGxm changes. The cast-sampler UV
re-anchor in `translator/texture.cpp` runs only above resolution 1, so it
is probably not the cause at 1x.

We do not know which version matches the real Vita.

## Plan

1. Compare a Vita screenshot of the title screen (from a video or from
   the user) with both builds.
2. Split the shader and SceGxm files of 8be36fa1 into groups and test them
   on our `master` with the title screen test.
3. Port only the change that gives the right colors.

## Comments
