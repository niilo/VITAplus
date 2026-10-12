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