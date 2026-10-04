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

- A quoted answer to step 1, with file and line. Done.
- Either a statement that the map is reachable, with the API that reaches it,
  or the file and function in dynarmic that blocks it. Done, in both directions:
  the map is reachable through `AddressSpace::ReverseGetLocation`, and the two
  declarations that would expose it are named.
- The per-thread unknown share from the fallback run. **Not done.** It needs the
  device, and it is the last thing this ticket is waiting on.

## Answer, steps 1 to 3

Steps 1 to 3 are answered from the code, so they do not need a device. Step 4
still does, so the ticket stays `open`.

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

**So step 2's answer is no on the `A32::Jit` class, and step 3 applies to that
class.** Naming what would have to change took a second pass, and the second
pass corrected the first: the map the first pass said did not exist does exist,
one layer down.

### The map exists, and it is public, one class below

`Dynarmic::Backend::Arm64::AddressSpace`
(`external/dynarmic/src/dynarmic/backend/arm64/address_space.h:25`) keeps the
reverse map in `reverse_block_entries`, a
`std::map<CodePtr, IR::LocationDescriptor>` at `address_space.h:79`. Its key is
the host entry point and its value is the guest-side `LocationDescriptor`. That
is exactly the host-PC-to-guest-PC direction this ticket needs.

It is populated in `AddressSpace::Emit`
(`external/dynarmic/src/dynarmic/backend/arm64/address_space.cpp:119-120`):

```cpp
ASSERT(block_entries.insert({block.Location(), block_info.entry_point}).second);
ASSERT(reverse_block_entries.insert({block_info.entry_point, block.Location()}).second);
```

`block_entries` is the forward map, guest PC to host address, and
`block_infos` at `address_space.h:80` is a third map keyed by the same
`CodePtr`.

Two methods read the reverse map and both are **public**, declared in the
`public:` section that runs from `address_space.h:26` to the `protected:` at
`address_space.h:48`:

| line | method | what it gives |
| --- | --- | --- |
| 35 | `std::optional<IR::LocationDescriptor> ReverseGetLocation(CodePtr host_pc)` | the guest descriptor for a host PC |
| 38 | `CodePtr ReverseGetEntryPoint(CodePtr host_pc)` | the host entry point for a host PC |

`ReverseGetLocation` is defined at `address_space.cpp:45`. It is a
`std::map::upper_bound` walk, so it answers for a host PC inside a block and not
only at an entry point, which is what a sampled instruction address needs.

The one in-tree caller is `address_space.cpp:321`, inside the block-info dump.
`ReverseGetEntryPoint` has no caller at all outside its own definition. So the
lookup is already used, and already proven to work, and it is not wired to
anything that a profiler could reach.

### What step 3 asks for, named exactly

`A32::Jit` keeps its state in a private pimpl, `std::unique_ptr<Impl> impl` at
`external/dynarmic/src/dynarmic/interface/A32/a32.h:105`. That `Impl` is defined
in the backend, not the interface:
`external/dynarmic/src/dynarmic/backend/arm64/a32_interface.cpp:24`. It holds
the address space as a member, `current_address_space(conf)` at
`a32_interface.cpp:28`, typed `A32AddressSpace`, which derives from
`AddressSpace`.

So the change is two declarations, not a new data structure and not a patch to
the emitter:

1. A public forwarding method on `Dynarmic::A32::Jit`, declared in
   `interface/A32/a32.h` beside the other public methods, defined in
   `backend/arm64/a32_interface.cpp` beside the other `Jit` methods, calling
   `impl->current_address_space.ReverseGetLocation(host_pc)`.
2. The same on `A64::Jit`, in `interface/A64/a64.h` and
   `backend/arm64/a64_interface.cpp`. The A64 `Impl` is at
   `a64_interface.cpp:25` and has the same `current_address_space` member.

The other three backends do not need it for this plan. `backend/x64/a32_interface.cpp:62`
and `backend/riscv64/a32_interface.cpp:24` are other hosts' code, and this
project targets arm64 Android and x86 Linux, where dynarmic also lowers to
arm64 via the same `backend/arm64` path.

**This corrects the step 2 answer above.** The first pass read only the `Jit`
class and concluded the information "is not on the way out". It is on the way
out of `AddressSpace`, and `AddressSpace` is one pimpl hop away. The first pass
was right that the `A32::Jit` public API has no such method, and that list in
step 1 is still accurate.

### What is still not settled

The host address a sample carries has to be turned into a `CodePtr` in dynarmic's
code cache before `ReverseGetLocation` will answer, and the code cache is
anonymous memory. simpleperf reported `unknown[+717551200c]`, which is an offset
from an unmapped or unnamed mapping. So the question this ticket's step 3 does
not answer, and which needs a device to answer, is whether the app can obtain
the base address of dynarmic's code cache at run time and match sample addresses
against it. Vita3K creates that cache, so the base is probably reachable from
the emulator's own side, but nothing in `vita3k/cpu/` reads it back today.

Step 4, the per-thread unknown share, is still worth running and still needs the
device: it bounds how much of the profile the missing map is worth, and that
number decides whether the two declarations above are worth anything.