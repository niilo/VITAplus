# 06: Measure zip play, and set the rule for deflated zips

Status: open
Type: experiment
Label: ready-for-agent
Blocked by: 04

## Question

Does playing from a zip cost speed or load time, and what should the app do with
deflated entries?

## Measurements (Ayaneo Pocket S, release APK, Turnip driver, resolution 2x)

Use the same game in four forms and the same scene:

| Form | How to make it |
| --- | --- |
| installed folder | the normal install |
| stored zip | `zip -0 -r` of the app folder |
| deflated zip | `zip -6 -r` |
| deflated zip, warm cache | the second start of the deflated zip |

For each: time from launch to the title screen (log timestamps), time from
"Continue" to the level, FPS in the scene (`tools/android/uncharted_scene.sh`,
compare `fps.png`), and the disk space used in `<cache>/zipfs`. Games: Uncharted:
Golden Abyss (many mid-size files) and Jak and Daxter Collection (one large
`jak1.psarc`, random reads). Repeat each run three times and report the median.
Accept a cost of 1 FPS or 5 percent load time for the stored zip. Above that,
find the cause (for example the `file_mutex` or the index lookup) and fix it.

## Policy to decide from the numbers

- A cache budget for deflated entries (setting `zip-cache-limit-mib`, default
  4096) with least-recently-used eviction of whole cache files that no open
  handle uses. A single entry bigger than the budget is still extracted (the
  game cannot run otherwise) and the log says so.
- A message in the apps list and in the add dialog when a zip has deflated
  entries above a size (default 128 MiB): "This zip is compressed. The first
  start needs time and space. Store the file without compression for best
  results." with the command `zip -0 -r`.
- Progress: the first read of a large deflated entry blocks the guest thread.
  Decide if a "Preparing game data" notice is needed (for example when a single
  extraction takes more than 2 seconds).
- ZIP64: test a zip with an entry above 4 GiB and more than 65535 entries (a
  generated sparse file is enough for the test, nothing is stored in the
  repository).

## Documentation

Add a short section to `README.md` or `docs/` ("Play from a zip"): accepted
layout, how to make a stored zip on Windows (7-Zip, "Store"), macOS and Linux
(`zip -0 -r`), that the zip must stay where it is, and the limits (issue 07).

## Done when

The table is filled in under `## Answer`, the policy is implemented or rejected
with a reason, and the documentation exists.

## Comments
