# Spec: play a game straight from a zip file

## Goal

A user adds a zip file that holds an app folder (a dump with `sce_sys/param.sfo`
and `eboot.bin`). The game shows in the apps list and starts without
installing. Nothing is extracted to `ux0/app`. The zip stays where it is.

## How it works today (facts from the code)

- Install: `install_archive` (`vita3k/interface.cpp:294`) opens a zip with
  miniz (`mz_zip_reader_init_cfile`), finds each content by its
  `sce_sys/param.sfo` (`get_archive_contents_path`), extracts all files to a temp
  folder, decrypts if `sce_sys/package/` exists (`is_nonpdrm`), and copies the
  folder to `ux0/app/<title id>` (`copy_path`).
- The apps list scans the folders under `ux0/app` (`collect_app_cache_sources`,
  `vita3k/app/src/apps_list.cpp:66`). It reads `sce_sys/param.sfo` through
  `vfs::read_app_file` (`apps_list.cpp:326`) and gets the icon as a host path.
- A game starts with `setup_game_launch` (`vita3k/app/src/app.cpp:250`). The
  loader reads `sce_sys/param.sfo` and loads `app0:eboot.bin`
  (`vita3k/interface.cpp:501` and after).
- The guest sees the app folder as `app0:`. `translate_path`
  (`vita3k/io/src/io.cpp:238`) turns `app0:` into `ux0:app/<title id>`.
- File access is built on host paths and `FILE *`: `open_file` (`io.cpp:333`)
  tests `fs::exists` and opens a `FileStats` that holds a `FilePtr`
  (`vita3k/io/include/io/state.h`, `io/filesystem.h`). `read_file_at` (`io.cpp:435`),
  `stat_file` (`io.cpp:687`), `open_dir` and `read_dir` (`io.cpp:882`, `928`) and
  the case-insensitive search `find_case_isens_path` (`io.cpp:193`) all use host
  paths.
- Other code reads the app folder on the host: `vfs::read_app_file` (module
  loading in `vita3k/modules/module_parent.cpp:300`, `SceAppMgr.cpp:423`), the
  trophy code (`vita3k/np/src/trophy/context.cpp:405`), the background picture
  (`app.cpp:277`), the app size on Android
  (`vita3k/android/jni/native_apps.cpp:189`) and the Qt live area.
- miniz 11.0.2 is in `external/miniz`. It reads ZIP64 and accepts a custom read
  callback.
- Android gives the install code a plain file path
  (`vita3k/android/jni/native_install.cpp:80`). The manifest asks for
  `MANAGE_EXTERNAL_STORAGE`, so a zip on the SD card is readable by path.

## Design

### Accepted zip

A zip is an app zip if it has `sce_sys/param.sfo` at the root, or under exactly
one top-level folder (for example `PCSA00029/sce_sys/param.sfo`). The title ID
comes from `TITLE_ID` in `param.sfo`, not from the file name. Phase 1 takes
only apps (category `gd`).

### Module `zipfs`

A new static library `vita3k/zipfs/`. One class, `ZipVolume`, hides the zip
format:

- `ZipVolume::open(path)` reads the central directory once (miniz, ZIP64) and
  builds an index: lower-case path to entry (offset of the data, sizes, method,
  CRC, time) and a folder tree. Lower case, because the Vita file system ignores
  case.
- `find(path)`, `list(folder)`, `read_at(entry, offset, buffer, size)`. It
  reads with a positional read on the file (`pread`, or `ReadFile` with an
  offset on Windows), so many threads can read at once without a lock.
- Stored entries (method 0) are read directly at their offset in the zip.
- Deflated entries cannot be read at a random offset. They are extracted once,
  on first open, to a cache file, and read from there. See issue 06.
- Everything the rest of the emulator needs from a zip goes through this class.

### Mount rule

A title ID is zip-backed when `ux0/app/<id>` does not exist on the host and a
registered zip holds that title. If the folder exists, the host folder wins (an
installed game, or a game that the user extracted).

### Writes and the overlay

The zip is never changed. A game that opens an `app0:` file for writing, or
creates a file or folder there, gets a host overlay in
`<vita fs>/ux0/app_overlay/<id>/` (not under `ux0/app`, so the apps list does
not see it as an installed game). A file that exists in the zip is copied to the
overlay on the first write (copy on write). Reads look in the overlay first. A
delete or rename of a zip file is refused with a file system error. The game
data in `savedata0:`, `ux0:user` and `ux0:data` is not affected: it already
lives on the host.

### Registry

`<config>/gui-configs/zip-apps.xml` lists the registered zips: title ID, path,
size, modification time. At every scan the app checks the file: a changed size
or time triggers a new index, a missing file shows the entry as "missing" and
does not delete it. The same file format holds a list of "games folders", which
the app scans for `*.zip` (phase 2, issue 03).

### Assets for the UI

The icon (`sce_sys/icon0.png`), the background (`sce_sys/pic0.png`) and the live
area files are extracted once to `<cache>/zipfs/<id>/` when a zip is
registered. One helper, `resolve_app_asset(id, relative path)`, returns the host
path (the real file or the cached one). The apps list, `app.cpp:277`, the Qt
live area and Android use it.

## Out of scope for phase 1

- Encrypted dumps (`sce_sys/package/work.bin`, NoNpDrm). The app says that the
  game needs an install. See issue 07.
- Updates (patches) and DLC for a zip-backed game. See issue 07.
- Zip files that Android can only open as a `content://` URI (Storage Access
  Framework). A path on the SD card or in a shared folder works.
- Other archive formats (7z, rar, tar), zips inside zips, and multi-part zips.
- Test data: no game content goes into the repository. Unit tests build their
  own small zip files. Device tests use the user's own dumps.

## Risks

- Hot path: `read_file_at` is used for every game read. The zip branch must not
  slow the host branch. Measure (issue 06).
- Deflated assets: a game with a 1 GB deflated archive file needs 1 GB of cache
  and a long first read. The remedy is to store the file (`zip -0`). The app
  must say so clearly.
- Thread safety of the index and the cache (two threads open the same deflated
  entry).

## Issues

1. `01-zipfs-volume.md`: the library.
2. `02-io-zip-backend.md`: use it in the file system layer. Blocked by 01.
3. `03-registry-and-apps-list.md`: add, scan and list zips. Blocked by 01.
4. `04-launch-from-zip.md`: start a game and fix the other host readers.
   Blocked by 02 and 03.
5. `05-ui-add-and-manage.md`: Android and Qt. Blocked by 03.
6. `06-performance-and-deflate-policy.md`: measure, cache budget, how to make a
   good zip. Blocked by 04.
7. `07-encrypted-updates-dlc.md`: decisions for what phase 1 leaves out.
