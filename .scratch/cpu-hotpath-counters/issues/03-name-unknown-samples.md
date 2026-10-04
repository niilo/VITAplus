# 03: Name the 6.05% of unknown samples

Status: open
Type: research
Label: ready-for-human
Blocked by: 01

## Question

Ticket 32 measured 6.05% of samples as three `unknown` clusters within 20 bytes
of each other, so one out-of-range mapping in one function. Can the JIT code
cache be mapped back to guest PCs, so those samples become attributable?

## Why it is not a code change yet

The public `Dynarmic::A32::Jit` API
(`external/dynarmic/src/dynarmic/interface/A32/a32.h`) exposes register access,
cache invalidation, halt and disassembly. It does not expose the host address
of a compiled block. Without that address, a sample cannot be turned into a
guest PC.

So the first step is to establish whether the information is reachable at all.

## Steps

1. Read the `Dynarmic::A32::Jit` public API and record every method that
   returns or exposes a compiled block's host address. Quote the file and line
   for each.
2. If any exists, the answer is that a guest-PC map can be built without
   touching the submodule, and the work moves to a new ticket.
3. If none exists, record that a dynarmic change is required, name the file and
   function in dynarmic that would have to change, and stop. Do not carry a
   submodule patch in this feature. `CLAUDE.md` already records submodule
   working-tree failures in worktrees, and this plan does not add to that.
4. Fallback that needs no code: run `device.sh perf` with a call graph and
   report the unknown share per thread. That bounds the prize without naming
   it, which is still worth having.

## Acceptance

- A quoted answer to step 1, with file and line.
- Either a statement that the map is reachable, with the API that reaches it,
  or the file and function in dynarmic that blocks it.
- The per-thread unknown share from the fallback run.