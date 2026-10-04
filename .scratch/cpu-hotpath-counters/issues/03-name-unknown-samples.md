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

## Answer, step 1 only

Step 1 is answered from the code, so it does not need a device. Steps 2 to 4
still do.

**No method on `Dynarmic::A32::Jit` returns or exposes a compiled block's host
address.** The full public API of `external/dynarmic/src/dynarmic/interface/A32/a32.h`:

| line | method | what it gives |
| --- | --- | --- |
| 41 | `ClearCache()` | nothing back |
| 48 | `InvalidateCacheRange(start_address, length)` | nothing back |
| 54 | `Reset()` | nothing back |
| 59 | `HaltExecution(hr)` | nothing back |
| 65 | `ClearHalt(hr)` | nothing back |
| 68 | `Regs()` | guest registers |
| 71 | `ExtRegs()` | guest extension registers |
| 74 | `Cpsr()` / `SetCpsr` | guest flags |
| 78 | `Fpscr()` / `SetFpscr` | guest flags |
| 82 | `ClearExclusiveState()` | nothing back |
| 88 | `IsExecuting()` | a bool |
| 93 | `DumpDisassembly()` | text |
| 99 | `Disassemble()` | text |

`DumpDisassembly` and `Disassemble` are the only two that look at compiled code,
and both return `std::string` or `std::vector<std::string>`. Neither returns an
address.

The block type `IR::Block` (`external/dynarmic/src/dynarmic/ir/basic_block.h:36`)
is an IR block built during translation. It holds a `LocationDescriptor`, which
is guest-side. The host address is assigned later, by the backend, after the IR
is lowered. So the information exists inside dynarmic and is not on the way out.

**So step 2's answer is no, and step 3 applies:** mapping the JIT output back to
guest PCs needs a change in dynarmic. This plan does not carry a submodule patch,
as the spec says, and `CLAUDE.md` already records submodule working-tree failures
in worktrees.

The file that would have to change is the one that lowers a finished block to
host code, in `external/dynarmic/src/dynarmic/backend/`. Naming it exactly is
step 3's remaining work and needs someone reading that backend.

Step 4, the per-thread unknown share, is still worth running: it bounds how much
of the profile the missing map is worth.