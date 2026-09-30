# 01: Build the `zipfs` library: open a zip as a read-only volume

Status: open
Type: task
Label: ready-for-agent

## Goal

A new static library `vita3k/zipfs/` (headers in `include/zipfs/`, code in
`src/`, wired into `vita3k/CMakeLists.txt` like the other libraries). It lets
the rest of the emulator read an app zip without knowing the zip format. See
`../spec.md`, section Design.

## Interface (keep it small)

```cpp
namespace zipfs {
struct Entry { uint64_t size; bool is_directory; /* private: offsets, method, crc, time */ };

class ZipVolume {
public:
    // Opens the zip and reads the central directory. Returns null and sets
    // `error` if it is not a readable zip or has no app root.
    static std::shared_ptr<ZipVolume> open(const fs::path &zip_path, std::string &error);

    const std::string &root_prefix() const;          // "" or "PCSA00029/"
    const Entry *find(std::string_view path) const;  // case-insensitive, path relative to the root
    std::vector<std::string> list(std::string_view folder) const;  // names, for readdir
    // Positional read. Returns the byte count, or -1. Safe to call from many threads.
    int64_t read_at(const Entry &entry, uint64_t offset, void *buffer, size_t size);
    // Extracts the entry to a cache file (for deflated entries) and returns its host path.
    fs::path cache_file(const Entry &entry);
    uint64_t total_size() const;                     // sum of the entry sizes
    fs::path path() const;  int64_t mtime() const;
};
}
```

## Behavior

1. **Root detection.** `sce_sys/param.sfo` at the root, or under exactly one
   top-level folder. Otherwise `open` fails with "not an app zip".
2. **Index.** Lower-case keys. Use `/` as the separator, strip a leading `./`.
   Folders that exist only as a prefix of a file name (many zips have no folder
   entries) are created in the tree.
3. **Reads.** Open the file once. Use miniz with a custom read callback
   (`mz_zip_archive::m_pRead`, `mz_zip_reader_init(pZip, size, flags)`) that calls
   `pread` (POSIX) or `ReadFile` with an `OVERLAPPED` offset (Windows). No lock
   for reads.
4. **Stored entries** (method 0): `read_at` computes the start of the data
   (local header offset + 30 + name length + extra length, read from the local
   header, not from the central directory) and reads there.
5. **Deflated entries**: `read_at` uses `cache_file`: extract once with
   `mz_zip_reader_extract_to_callback` to
   `<cache>/zipfs/<zip hash>/<entry hash>`, write to a temp name and rename when
   complete, check the CRC, and serve later reads from the file. Two threads that
   want the same entry wait for one extraction (a per-entry `std::once_flag` or
   mutex). Policy (budget, eviction) is issue 06.
6. **ZIP64** and names in UTF-8 work. Encrypted entries (the zip password flag)
   are refused.
7. **Errors** never crash. A truncated or damaged zip returns an error string.

## Tests (googletest, new target `zipfs-tests`, like `mem-tests`)

Build zips in the test with miniz's writer. Do not add game data.

- root at `/`, root under one folder, two folders (refused), no `param.sfo`
  (refused);
- a zip without folder entries;
- stored and deflated entries of 0 bytes, 1 byte, a few KiB and 5 MiB: read the
  whole entry and read in the middle and at the end (`read_at` with offsets), and
  compare with the source bytes;
- case-insensitive `find`, `list` of the root and of a sub folder;
- 8 threads read the same stored entry and the same deflated entry at once;
- a truncated zip and a file that is not a zip;
- a zip with data before the zip (an offset start) if miniz accepts it.

Register the target in `CMakePresets.json`/ctest like the other suites and add
it to `container/vita3k.sh test`.

## Done when

`container/vita3k.sh test` runs `zipfs-tests` and they pass, and
`container/vita3k.sh format-check` passes.

## Comments
