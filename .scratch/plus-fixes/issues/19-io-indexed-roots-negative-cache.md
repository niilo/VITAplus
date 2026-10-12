# 19: A lookup for a missing file re-walks the whole application directory

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/io/src/io.cpp:218-223`:

```cpp
if (!fs::exists(final_path))
    return false;

for (const auto &file : fs::recursive_directory_iterator(final_path)) {
    io.cachemap.emplace(string_utils::tolower(file.path().string()), file.path().string());
}

return true;
```

`final_path` here is the application root, app0 or addcont0. There is no record
of which roots have already been indexed, so a lookup for a file that does not
exist walks the entire directory again. On a title with several thousand files,
every open of a missing file costs a full directory walk, on the load path.

On Android with the game on external storage this is the largest load-time cost
in the list.

The fix depends on ticket 18. The new set of indexed roots would sit next to
`cachemap`, so adding one without the other leaves the two guarded
inconsistently and the data race in `cachemap` untouched.

Vita3K-Plus commit `7e6e22a4`.

## Plan

1. Add an `indexed_roots` set to `IOState` and check it before walking. Clear it
   in `io_deinit` alongside `cachemap` at `io.cpp:166`.
2. Take `cachemap_mutex` in both the indexing function and the lookup function,
   as ticket 18 does. The set and the map must be guarded by the same lock.
3. Do not hold the lock across the directory walk itself if that can be avoided,
   since it is long and the lookup path is hot. Populate a local map and take
   the lock to merge it.
4. Measure before and after with a cold launch on a title with many files, and
   record the time to first frame.

## Comments