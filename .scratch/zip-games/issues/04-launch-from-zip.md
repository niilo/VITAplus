# 04: Start a game from a zip and fix the other host readers

Status: open
Type: task
Label: ready-for-agent
Blocked by: 02, 03

## Goal

Selecting a zip game starts it and it plays like an installed game.

## Steps

1. `setup_game_launch` (`app.cpp:250`) and `set_app_info` (`apps_list.cpp:569`):
   for a zip-backed title, open the `ZipVolume` (from the registry), set
   `emuenv.io.app_zip` and `app_overlay_path`, then continue as today. Close the
   volume when the game ends (`app.cpp:346` clears `io.app_path`).
2. `load_app` (`interface.cpp:499` and after): `vfs::read_app_file` for
   `sce_sys/param.sfo` and the module loading already go through `vfs` after
   issue 02. Check `load_module` in `module_parent.cpp:300` and the LLE module
   search for modules inside `app0:` (for example `app0:sce_module/*.suprx`).
3. Replace the direct host reads of the app folder:
   - `app.cpp:277`: the background `pic0.png` -> `resolve_app_asset`.
   - `vita3k/np/src/trophy/context.cpp:405`: the trophy file `sce_sys/trophy/*.trp`
     -> read through `vfs` (it builds a normalized `app0:` path today).
   - `modules/SceAppMgr/SceAppMgr.cpp:423`: already `vfs`; test `sceAppMgr`
     launching another executable of the same app (the Jak and Daxter Collection
     does this: `Jak1.self`).
   - Qt `live_area_widget.cpp` and the live area files: `resolve_app_asset`.
   - `vita3k/android/jni/native_apps.cpp:189`: the app size. For a zip use
     `ZipVolume::total_size()`.
4. `addcont0:`, `savedata0:` and the patch folder `ux0/patch` are not part of
   this. Confirm by test that they still work (see issue 07 for updates).
5. The title-specific shader and pipeline caches and the `config_<id>.xml` use the
   title ID only. Confirm that a zip game and an installed game of the same title
   share them (good).

## Checks on the device (user's own dumps, none in the repository)

Make a stored zip of an installed game on the host
(`cd ux0/app/<id> && zip -0 -r ../<id>.zip .`), move the installed folder away,
register the zip, and start the game:

1. Uncharted: Golden Abyss (`PCSA00029`): title screen, Continue, the saved
   chapter (use `tools/android/uncharted_scene.sh`).
2. Jak and Daxter Collection (`PCSA00080`): it reads a 1 GB `jak1.psarc` and
   starts a second executable.
3. A game that writes into `app0:` if one is known; otherwise skip.
4. Compare the load time and FPS with the installed game (issue 06).

## Done when

Two real games start from a stored zip and reach the same screens as the
installed game, and the log shows no `Missing file` error that the installed game
does not show.

## Comments
