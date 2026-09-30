# 07: Decide what to do with encrypted dumps, updates and DLC of zip games

Status: open
Type: grilling
Label: ready-for-human

## Question

Phase 1 plays a decrypted app folder from a zip, read-only with a host overlay
for writes. Three cases are left out. For each, pick one option.

### A. Encrypted dumps (`sce_sys/package/work.bin`, NoNpDrm)

Today `install_archive` decrypts these with `is_nonpdrm` while it installs.

1. **Refuse with a clear message** ("install this game"). Simple. Recommended for
   phase 1.
2. **Decrypt to the cache on first start.** The game runs, but it needs the
   full game size of cache space, and it is no longer "play straight from the
   zip". Do it in a later phase only if users ask.

### B. Updates (patches)

Vita3K installs an update into the same app folder. A zip game has no folder.

1. **Not supported.** The user installs the game instead. Recommended for phase 1.
2. **Overlay.** An update zip (or installed update) is stacked on the base zip
   as a second read-only layer. The volume list becomes an ordered stack. More
   code in issue 02, but the overlay design there already has a stack shape.

### C. DLC and `addcont0:`

DLC lives in `ux0:addcont/<id>`, not in the app folder, so it already works
with a zip game. Confirm with one game that has DLC. No decision needed unless
the test fails.

## Also decide

- Should a zip entry that duplicates an installed game ask the user to delete
  the install? Default: no, the installed folder wins and the list shows a note.
- Should the app offer "Install this zip" for a registered zip game (extract and
  keep)? Default: yes, later, using the existing `install_archive`.

## Answer

(Fill in.)

## Comments
