# 07: Check the visibility query copy for the Adreno hang

Status: resolved
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

### The flags, with the file and line

`vita3k/renderer/src/vulkan/context.cpp:611-613`, inside `stop_recording`:

```cpp
render_cmd.copyQueryPoolResults(current_visibility_buffer->query_pool, range.offset, range.size,
    current_visibility_buffer->gpu_buffer, current_visibility_buffer->buffer_offset + range.offset * sizeof(uint32_t),
    sizeof(uint32_t), vk::QueryResultFlagBits::eWait);
```

**It passes `eWait` and nothing else.** No `e64`, no `eWithAvailability`, no
`eWithCount`. Confirmed on this base; it matches what the ticket said `master` at
`d9037c16` does, so nothing changed in the port.

The per-scope `resetQueryPool` is at `context.cpp:607`.

### The query pool, with the file and line

`vita3k/renderer/src/vulkan/sync_state.cpp:230-234`:

```cpp
vk::QueryPoolCreateInfo pool_info{
    .queryType = vk::QueryType::eOcclusion,
    .queryCount = static_cast<uint32_t>(stride / sizeof(uint32_t))
};
vk::QueryPool query_pool = context.state.device.createQueryPool(pool_info);
```

Query type is occlusion, and **no pipeline statistics are enabled at all**, so
none of the `ePipelineStatistics` result flags apply here. The ticket asked for
the pipeline statistics; the answer is that there are none.

`beginQuery` is at `scene.cpp:487`, and it passes `ePrecise` only when both the
game asked for an increment and the device reports
`occlusionQueryPrecise` (`scene.cpp:486`). On this device that depends on the
driver; either way the scope is closed with `endQuery` before a different index
is used (`sync_state.cpp:266`, `:277-280`).

### The visibility index fix from log-review issue 02 is in this base

Two bounds checks are present, and they are the fix:

- `sync_state.cpp:248-251`, on the buffer being swapped in, which is the case
  the old note described ("does not check the index while no buffer is set").
- `sync_state.cpp:261-264`, inside `sync_visibility_index`, on every call.

Both clamp to 0 and log `LOG_WARN_ONCE`. The pool range is also checked
indirectly: a range is only emitted for entries where `queries_used` is true,
and those entries come from indices that passed the check above.

### Verdict: the flags are the ones that hang, but this base does not have the hang

**The flags are the ones RPCS3's hang used: `eWait`, with no availability bit.**
Danylo Piliaiev, the Mesa developer who read valpackett's GPU core dump, gave
the cause in RPCS3 PR 19561 (quoted in the PR):

> The hang happens due to vkCmdCopyQueryPoolResults with
> VK_QUERY_RESULT_WAIT_BIT waiting for availability bit which never happens

and the devcore sequence he listed shows `vkCmdCopyQueryPoolResults +
VK_QUERY_RESULT_WAIT_BIT` waiting infinitely on a query whose
`available = 1` had not yet been written, because `begin/end query_2` came
*after* the copy. RPCS3 fixed it by forcing strict query scopes on Adreno and
other tilers, so the copy cannot be issued before the scope closes.

**This base issues the copy after the scope closes, which is the condition that
avoids the hang.** `stop_recording` runs the copy after the whole scene's
`beginQuery`/`endQuery` pairs are recorded, and the ranges it copies are built
from `queries_used`, which `scene.cpp:482` sets only when `beginQuery` actually
runs. A query that was never begun is never copied, so there is no wait on an
availability bit that has not been written. So the shared preconditions for the
RPCS3 hang are not present.

That is a code-reading verdict, not a measurement: no title was run for this
ticket, and step 4's A/B was not done. **Step 4 was deliberately not done.** It
puts `eWait` behind a config value, and the ticket is explicit that a run with
the wait off is not a correct picture. On the evidence above that measurement
would only confirm a hang this base should not have, so it is not worth a
device session or a config value that ships a wrong picture as a default.

So: **safe as is, no config value needed.**

### A real out-of-bounds read found while checking step 3, and fixed

This is not what the ticket asked about, but it is in the code it asked me to
read, and it is a genuine bug.

`sync_state.cpp:241` sizes the tracking vector with a spare element:

```cpp
// the + 1 is to make computing the ranges easier in context.cpp
ite->second.queries_used.resize(ite->second.size + 1, false);
```

`context.cpp:592` relies on that spare element, because the loop bound is
inclusive of one past the last real query:

```cpp
for (uint32_t entry = 0; entry <= visibility_max_used_idx + 1; entry++) {
    if (current_visibility_buffer->queries_used[entry] == in_range)
```

But `context.cpp:616` reset the vector with `assign(size, false)`, which
**shrinks it back to `size` elements**. After the first scene, the loop can
therefore read `queries_used[size]`, one past the end. `visibility_max_used_idx`
reaches `size - 1` whenever the game's last visibility slot is used, which is
the common case, so this is not a corner.

The fix is one line, `context.cpp:616`, now `assign(size + 1, false)`.

It was not caught by the validation layer because `operator[]` on a
`std::vector<bool>` reads one bit past the end, which is usually the next
allocation's bit and is almost always `false`, so the range loop reads it as
"end of range" and produces a correct-looking range list. It is undefined
behaviour and it can produce a wrong picture silently, which is the same
failure mode this ticket's risk section warns about.

## Comments
