# 18: io.cachemap is written and read with no lock

Status: open
Type: task
Label: ready-for-agent

## Context

`io.cachemap` (`vita3k/io/include/io/state.h:124`) is an
`std::unordered_map<std::string, std::string>` mapping a lowercased path to the
real one. Three sites touch it:

- `vita3k/io/src/io.cpp:222` writes it, one entry per file found under the
  application root.
- `vita3k/io/src/io.cpp:229` reads it, on every path translation.
- `vita3k/io/src/io.cpp:166` clears it on teardown.

None of them takes a lock. `grep cachemap_mutex vita3k/` returns nothing, so
the member does not exist.

Path translation runs on every guest file call, and the population at `:222`
runs while the guest is loading. A guest thread that opens a file while the
loading thread is still walking the directory reads the map while it is being
rehashed. That is a use-after-free on the bucket array.

The write at `:222` is inside a throwing
`fs::recursive_directory_iterator(final_path)`. A permission error or a broken
symlink under the application root throws out of `find_case_isens_path`, through
`translate_path`, into the guest.

Vita3K-Plus commit `7e6e22a4` adds the lock; `129cc7b2` adds the error-code
iterator.

## Plan

1. Add `cachemap_mutex` to `IOState` and take it in all three functions.
   Confirm no caller already holds it, and that no path takes
   `state->mutex` and then `cachemap_mutex` in the opposite order elsewhere.
2. Use the `error_code` overload of `fs::recursive_directory_iterator` at
   `:220` so a directory error is reported instead of thrown.
3. Test: load a title and translate a path from a second guest thread while the
   directory walk is in progress. Run under a sanitizer build if one is
   available; otherwise this is a correctness change argued from the code, not
   a reproduced failure, and the answer should say so.

## Comments