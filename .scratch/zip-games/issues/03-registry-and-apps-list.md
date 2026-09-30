# 03: Register zip games and show them in the apps list

Status: open
Type: task
Label: ready-for-agent
Blocked by: 01

## Goal

A registered zip appears in the apps list like an installed game, with its icon,
title and compatibility badge, and with a mark that it runs from a zip.

## Steps

1. **Registry** in `vita3k/app/` (new `zip_apps.h/.cpp`): load and save
   `<config>/gui-configs/zip-apps.xml` (pugixml, like `apps-cache.xml` in
   `apps_list.cpp`). Entry: title ID, zip path, size, modification time. Functions:
   `add_zip_app(emuenv, zip_path)` (opens the zip with `ZipVolume::open`, reads
   `sce_sys/param.sfo` from it, takes the title ID, refuses a category other
   than `gd` and an encrypted dump, extracts the assets, saves),
   `remove_zip_app(emuenv, title_id)`, `get_zip_apps(emuenv)`.
2. **Duplicates.** If `ux0/app/<id>` already exists, the zip entry is kept but
   the list shows the installed game only (the host folder wins). If two zips hold
   the same title ID, the newest registration wins and the other shows as a
   duplicate in the log.
3. **Scan.** `scan_apps` (`apps_list.cpp:240`) and `collect_app_cache_sources`
   (`:66`) also take the registered zips. A zip whose size or time changed is
   indexed again. A missing zip is listed with the state "missing" (greyed out, no
   start) and is not deleted.
4. **AppEntry.** Add `bool from_zip` and `fs::path zip_path` (and a
   `missing` flag) to `AppEntry` in `vita3k/app/include/app/state.h`. Bump
   `CACHE_VERSION` (`apps_list.cpp:34`) and read and write the new fields in
   `read_app_cache_entry` and `write_app_cache_entry`.
5. **Assets.** Write `resolve_app_asset(emuenv, title_id, "sce_sys/icon0.png")` in
   `app/`. For an installed app it returns the real path. For a zip app it
   returns `<cache>/zipfs/<id>/<file>`, extracting on first use. Use it in
   `get_icon_path` (`apps_list.cpp:210`). The other callers come in issue 04.
6. **Phase 2 (optional, same issue after the rest works).** A "games folder"
   setting: a list of host folders that the scan searches for `*.zip` (not
   recursive, and only files whose name or content matches an app zip). New zips
   are registered on their own.
7. **Tests.** The registry reads and writes, handles a missing zip, a changed zip
   and a duplicate. Use a temp config folder and synthetic zips.

## Done when

With a zip registered by a test or by a small debug command line option, the
apps list (Qt and Android) shows the game with its icon after a restart and
after a rescan, and the tests pass.

## Comments
