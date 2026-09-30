# 02: Serve `app0:` from a zip in the file system layer

Status: open
Type: task
Label: ready-for-agent
Blocked by: 01

## Goal

When a title is zip-backed, every guest file call on `app0:` reads from the
`ZipVolume`. The host branch stays as fast as it is today. See `../spec.md`
(Mount rule, Writes and the overlay).

## Where the code changes

All in `vita3k/io/`:

- `include/io/state.h`: `FileStats` owns a `FilePtr` (`FILE *`) and `DirStats`
  owns a `DIR *`. Give them a second kind of source. Suggested form: a small
  `FileSource` interface with two implementations, `HostFile` (today's code) and
  `ZipFile` (an `Entry` and the volume). `FileStats::read`, `seek`, `tell`,
  `truncate`, `write` call the source. Keep `get_file_pointer()` for the host
  source only (the one other user is `SceIofilemgr.cpp:143`, which only reads the
  vita location string, so check that it stays valid).
- `IOState` gets `std::shared_ptr<zipfs::ZipVolume> app_zip` (set for the running
  title) and `fs::path app_overlay_path`.
- `src/io.cpp`:
  - `open_file` (`:333`): after `translate_path` for a device that maps to
    `ux0:app/<id>/...`, try the overlay (host), then the host folder, then the
    zip. A write open of a zip file copies it to the overlay first (copy on
    write). `SCE_O_CREAT` creates in the overlay.
  - `read_file_at` (`:435`), `read_file`, `read_file_into_guest` (`:481`),
    `seek_file`, `tell_file`: through the source. The zip source's `read_at` is
    positional, so concurrent reads do not need `file_mutex`.
  - `stat_file` (`:687`) and `stat_file_by_fd`: size and mode from the entry,
    times from the zip entry (DOS time), `SCE_S_IFREG`/`SCE_S_IFDIR`.
  - `open_dir`/`read_dir`/`close_dir` (`:882`, `:928`): merge the overlay and the
    zip names (overlay wins), hide nothing else.
  - `remove_file`, `rename`, `remove_dir`, `truncate_file` on a zip-only path:
    return `SCE_ERROR_ERRNO_EACCES` and log once.
  - `find_case_isens_path` (`:193`) is not needed for the zip (the index is
    already case-insensitive) but must keep working for the overlay.
  - `vfs::read_app_file` and `vfs::read_file` (`:85`): when the path is under the
    zip-backed app, read from the volume. `get_directory_used_size`
    (`SceAppUtil.cpp:350` calls it for save data only) stays host only.
- `sce Fios` overlays (`create_overlay`, `resolve_path`) work on strings and end
  in `open_file`, so they need no change. Check that with a test.

## Tests (new target `io-tests`, googletest)

Use a real temp folder as `vita_fs_path` and a synthetic zip from issue 01:

- open, read, seek, tell, stat of a stored and of a deflated entry through the
  `open_file` API (no emulator state needed beyond `IOState`);
- directory listing with overlay and zip entries, names with different case;
- write to a zip file creates the overlay copy, later reads return the new bytes,
  the zip file on disk is unchanged (compare its hash);
- delete and rename are refused;
- a title whose `ux0/app/<id>` exists on the host ignores the zip.

## Done when

The new tests and the existing suites pass, and a build for Linux and for
Android passes.

## Comments
