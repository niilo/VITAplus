# 20: A config.yml written before guest-cores existed forces one core

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/config/src/settings.cpp:209`:

```cpp
out.guest_cores = gpu.attribute("guest-cores").as_int(1);
```

The built-in default is 3, at `vita3k/config/include/config/config.h:180`:

```cpp
code(int, "guest-cores", 3, guest_cores)
```

`as_int(1)` ignores that. A `config.yml` written before the key existed has no
`guest-cores` entry, so it gets 1, not 3. There is no warning and nothing in
the UI shows which value is in force, so the symptom is a title running slower
than it should.

Every other setting in that block has the same shape, so this is a pattern
rather than one key. Only this one has a default that differs from the
fallback.

Vita3K-Plus commit `5bfca771`, which pairs the fix with the UI work that lets a
user set it.

## Plan

1. Use the built-in default: `as_int(out.guest_cores)`, matching how the rest
   of `apply_gpu_settings` reads its values.
2. Audit the rest of `vita3k/config/src/settings.cpp` for `as_int` and
   `as_bool` calls whose fallback differs from the `code(...)` default in
   `config.h`. Each of those silently overrides the documented default for a
   config written before the key existed. Record the list here.
3. This ticket only pays off with the UI work from `5bfca771`. Without it the
   user has no way to set the value at all. Say which of the two is being done.

## Comments
### 2026-10-13: fixed guest-cores; one more mismatch found, left alone

Changed `as_int(1)` to `as_int(out.guest_cores)` in `fix/mem-kernel-bookkeeping`.

The audit in step 2 turned up a scriptable comparison of every
`as_int`/`as_bool` in `settings.cpp` against the `code(...)` default in
`config.h`. Twenty-eight keys read the built-in default already. Three pass a
fallback equal to the documented default, which is harmless but means the two
lists can drift: `accurate-thread-scheduling`, `preempt-on-wake-us` and
`shader-cache` and `texture-cache` and `validation-layer`.

One more mismatch beyond guest-cores:

- `validation-layer`: `config.h` says `false`,
  `settings.cpp:252` says `as_bool(true)`.

Left as it is. `5bfca771` carries the same discrepancy, so the two repositories
agree on it and it may well be deliberate: the validation layer being on when a
config does not mention it is a defensible default even if `config.h` says
otherwise. Turning it off is a behaviour change for every existing user on the
strength of a guess about intent, which is not worth it. Worth a decision from
whoever owns the config surface.

Three keys have no `code(...)` entry at all: `enable-ngs`,
`stretch-the-display-area`, `fullscreen-hd-res-pixel-perfect`. They read a
default that is only in `settings.cpp`, so a value written to the config is not
round-tripped through the same list. Not a default mismatch, so not fixed here.
