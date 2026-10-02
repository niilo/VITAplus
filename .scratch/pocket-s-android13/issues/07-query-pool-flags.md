# 07: Check the visibility query copy for the Adreno hang

Status: open
Type: research
Label: ready-for-agent
Blocked by: 01

## Question

`VKContext::stop_recording` calls `copyQueryPoolResults`
(`vita3k/renderer/src/vulkan/context.cpp:599`) once per scene. On Adreno this
call has hung other emulators. Does it carry the flags that hang?

## Why it matters

RPCS3 merged PR 19561, "force strict query scopes on Adreno and other tilers",
after games hung within seconds of going in-game. The hang was
`vkCmdCopyQueryPoolResults` with `VK_QUERY_RESULT_WAIT_BIT` waiting for an
availability bit that never arrived. A Mesa developer gave the cause in that
issue and recommended strict query scopes for tiling GPUs. Name the
developer in `## Answer`. RPCS3 issue 18828 reports the
same signature on Adreno 830 with Turnip.

This is a correctness risk before it is a performance one, and it is cheap to
check.

## Steps

1. Read `copyQueryPoolResults` at `context.cpp:599` and write down the flags it
   passes. On `master` at `d9037c16` it passes `eWait` at `context.cpp:601`
   and no availability bit. Confirm on this base, then go to step 4.
2. Read the query pool creation in
   `vita3k/renderer/src/vulkan/sync_state.cpp`, which is in the
   `createQueryPool` call after the `pool_info` declaration. Record the pipeline
   statistics it enables and the query type.
3. Read the visibility index handling. Issue 02 of
   `.scratch/log-review-2026-09-29/` already fixed an uninitialized
   `visibility_index`; confirm that fix is in this base and that the pool range
   is checked against the buffer.
4. `eWait` is what the renderer depends on for a correct picture. Put it behind
   a temporary config value `visibility-query-wait`, default true, and set it to
   false only to measure the stall. A run with it false is not a correct
   picture: the occlusion buffer holds values the GPU has not written, so
   geometry is culled wrongly, which shows as missing sprites or particles and
   does not crash. Take screenshots at three fixed moments per title in A and B
   for every title that uses the visibility buffer, and write down every
   difference. Ship only the default.

## Risk

This ticket can make the picture wrong without crashing or tripping the
validation layer. Anything other than the current default is a measurement, not
a candidate.

**Driver scope.** The plan measures on Turnip, and the hang this ticket is about
was reported on Turnip: RPCS3 issue 18828 is Adreno 830 with Turnip 26.1.0, and
PR 19561 fixed it by forcing strict query scopes on Adreno and Turnip. So this
check belongs on Turnip. Ticket 01 recorded the stock driver's extension list,
which has `VK_EXT_host_query_reset` and nothing that changes the picture; leave
the stock driver out.

## Acceptance

- The flags are written down with the file and line.
- A verdict: safe as is, or a config value that drops the wait.
- If a title hangs or shows artifacts on the stock driver or on Turnip, record
  it here, because that changes how the other tickets read their results.

## Answer

## Comments
