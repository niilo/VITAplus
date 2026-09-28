# Map: Ayaneo Pocket S optimization

The spec is `spec.md`. The tickets are in `issues/`. This map was reviewed
on 2026-09-25 by four review agents (code facts, technical design,
execution, device research). Their findings are built in.

## Ticket fields

Each ticket starts with these lines:

| Field | Values |
|---|---|
| `Status:` | `open`, `claimed`, `resolved`, `rejected` |
| `Type:` | `research`, `grilling`, `task`, `experiment` |
| `Label:` | `ready-for-agent` or `ready-for-human` (roles from `docs/agents/triage-labels.md`) |
| `Blocked by:` | ticket numbers, or `none`. Work may start when all of them are `resolved` or `rejected`. |
| `Measure after:` | optional. Ticket numbers that must be `resolved` before the ticket's device measurement. The code part may be done first. |

`rejected` means the ticket ran and its idea did not help or was not
possible. It counts as done for `Blocked by`.

## How an agent runs this plan

1. **Find the frontier.** List the tickets with `Status: open` whose
   `Blocked by:` tickets are all done. Take the lowest number. A
   `ready-for-human` ticket is on the frontier too: the agent claims it,
   does the agent steps, and asks the user for the human steps.
2. **Claim.** Set `Status: claimed` and add `Claimed: <date> <agent or
   session name>`. Commit and push this change to `master` before any work,
   so other agents see it. If the push fails because another agent pushed a
   claim for the same ticket, pull, and take the next ticket.
3. **Branch.** Work on `pocket-s/NN-<slug>`, created from `master`.
4. **Device lock.** There is one device. Before any device step, check for
   `tmp/device.lock`. If it exists, do the code steps of another ticket, or
   wait. If not, write the ticket number into it. Delete it when the device
   steps are done. Run `device.sh release` (ticket 04) at the end, so the
   emulator does not stay in memory.
5. **Code first.** A ticket with `Measure after:` can have its code done
   and pushed on its branch before those tickets are done. Leave it
   `claimed` until the measurement is done.
6. **Resolve.** Write the result under `## Answer`, with the commit hashes.
   Set `Status: resolved` or `rejected`. Add one line to "Decisions so far"
   below, with a link to the ticket. Commit and push.
7. **Merge.** Merge to `master` only the changes that the ticket keeps.
   Temporary config values used only for A/B runs stay on the ticket
   branch, or are removed before the merge. Push `master`. Delete the
   ticket branch after `## Answer` lists its commits.
8. **Stop rule.** If two experiments in the same area give no gain, stop
   work in that area. Write what was learned and move on.

## Human sessions

Human input is collected in three sessions, so the user is asked less
often. The agent asks for a whole session at once.

| Session | When | What the user does |
|---|---|---|
| H1 | After tickets 04 and 05 | Choose 4 titles and one scene each; give the save slot and the exact button sequence from boot to the scene; choose the Ayaneo mode for all tests; install the games and the Turnip driver in the release package; do the button test of ticket 17. |
| H2 | When ticket 01 is resolved | Make the ticket 02 decision. |
| H3 | When tickets 12 and 14 have screenshots | Judge picture problems and rank picture quality from the screenshots in `tmp/`. |

## Dependency order

```
01 research ──> 02 decide (H2) ──────────────────────────┐
04 harness ─┐                                             v
05 frame log┴─────────────────────────────────────> 06 baseline (H1)
03 validation default ─ measured in 06                    │
08 vblank clock, 11 present mode: code now, measure after 06
                                                          v
                                                    07 profile
                    ┌──────────────┬───────────┬──────────┼──────────┬──────────┐
                    v              v           v          v          v          v
               09 dynarmic   10 mapping   12 pipelines  13 threads  15 textures
                                  │
                                  v
                            14 resolution
    03, 08, 09, 10, 11, 12, 13, 14, 15 ──> 16 preset ──> 18 verify
    04 ──> 17 controls (H1) ─────────────────────────────> 18
```

## Notes: facts found in the code

Checked by a fact-check agent on 2026-09-25.

- **The Plus code is not in this repo.** The merge `1f75257f` took only
  `README.md`, `Screenshots.md`, screenshots and issue templates. So the
  README's "Accurate Thread Scheduling" and "Page Table" Android default do
  not exist here. `plus/all-enhancements` has 116 commits (plus one merge)
  that `master` lacks. `master` has 73 commits that Plus lacks. Plus is 13
  commits behind `upstream/master` as last fetched (`120671a4`).
- **Earlier Adreno work exists on three unmerged branches** by the user:
  `feat/ayaneo-pocket-s-performance` (`1838cd88`: v-sync-aware present mode
  selection, a pipeline cache key with driver identity, validation layer off
  on Android), `feat/vulkan13-adreno` (`be6c7690`, `d9f38adf`: Vulkan 1.3
  paths), and `feat/vulkan-device-profiles` (`dfb60519` and fixes: device
  profiles, FSR fallback). They are also on `origin`.
- **The validation layer is on by default** (`config.h:136`,
  `EmulatorConfig.kt:81`), and every APK carries it (`android/prebuilt/`).
  Android loads a layer packaged in the APK for release apps too. If the
  driver offers a debug extension, validation runs
  (`vulkan/renderer.cpp:430-460`). This can cost a lot of speed.
- **No Android-specific defaults exist.** Defaults are in
  `config/include/config/config.h:130-216`: Vulkan, resolution 1.0,
  `memory-mapping: double-buffer`, `async-pipeline-compilation: true`,
  `turbo-mode: false`, `screen-filter: Bilinear`.
- **App code does not read `Build.MODEL`, `Build.SOC_MODEL` or system
  properties.** Only SDL logs `Build.MODEL` (`SDLActivity.java:347-349`).
- **Kotlin settings map to native by hand** in
  `vita3k/android/jni/native_config.cpp` (field IDs `:128-160`, reads and
  writes near `:317` and `:459`). A new setting also needs the copy,
  `equals` and `hashCode` parts of `EmulatorConfig.kt`.
- **Files on the device.** Root is `getExternalFilesDir(null)` =
  `/sdcard/Android/data/<package>/files/` (`AppStorage.kt:11-13`). The log
  is `vita3k.log` and the config is `config.yml` in that folder
  (`util/src/logging.cpp:42`). `config.yml` is written on the first
  `NativeLib.init` (`config/src/config.cpp:321-327`, `:473-476`). Per-game
  files are `config/config_<titleid>.xml` and override `config.yml`
  (`config/src/settings.cpp:150-152`). `adb pull` works without `run-as`.
- **v-sync does nothing on Vulkan.** `set_vsync_state` stores
  `pending_vsync` (`app_init.cpp:587,680`), but only OpenGL reads it
  (`gl/renderer.cpp:755`). Vulkan picks the present mode once in `setup()`
  (`vulkan/screen_renderer.cpp:208-228`) and logs it (`:229`). The
  swapchain has `minImageCount + 1` images (`:258`). `vita_surface` is
  sized once to `swapchain_size` (`:684-685`) and indexed per image
  (`vulkan/renderer.cpp:1159`). A rebuild path exists (`:696-708`).
- **The vblank thread** runs at `1000000 / 60` µs per frame (60.002 Hz;
  the Vita is about 59.94 Hz) and reads `system_clock`, which is not
  monotonic (`display/src/display.cpp:32`, `:78`). It is not tied to the
  host display.
- **Frame metrics** (`app/src/app.cpp:289-340`): one FPS and one average
  ms value per second. The 20-sample ring holds FPS values. No per-frame
  time is kept. Frames are counted at `modules/SceDisplay/SceDisplay.cpp:159`.
- **Pipeline cache file** holds only a magic number, the pipeline hashes
  and the Vulkan cache blob (`vulkan/pipeline_cache.cpp:281-330`). It
  cannot be used to build pipelines before first use. Cached hashes are
  compiled on the render thread on purpose (`:951`, `:978`). A new pipeline
  with async compile on is skipped for that draw (`vulkan/scene.cpp:409-411`).
  3 compile workers on 8 cores (`:228-239`).
- **Texture cache** always uses page protection on Vulkan
  (`vulkan/renderer.cpp:983` passes `true`). PVRTC is decoded on the CPU
  (`texture/cache.cpp:460-473`). Uploads can wait on fences
  (`vulkan/texture.cpp:204,225`; per frame at `vulkan/context.cpp:579`).
- **Memory mapping.** Modes allowed on the device are in
  `supported_mapping_methods_mask` (`vulkan/renderer.cpp:694-715`). Only the
  chosen mode is logged (`:974`). The settings list shows the allowed modes
  (`SettingsRepository.kt:115`). Page Table and Native Buffer turn off
  dynarmic fastmem (`app/src/app_init.cpp:565`,
  `cpu/src/dynarmic_cpu.cpp:339-344`).
- **Dynarmic** uses `all_safe_optimizations` (`cpu/src/dynarmic_cpu.cpp:349`).
  `fastmem_exclusive_access` is off (dynarmic default). Each guest thread
  has its own JIT with a 128 MiB code cache (`kernel/src/thread.cpp:74`).
- **No host thread priority or affinity** is set. Guest threads map 1:1 to
  `SDL_CreateThread` (`kernel/src/kernel.cpp:168`).
- **Android APIs.** Not used: `setFrameRate`, ADPF, thermal status,
  sustained performance, Game Mode. Used: `appCategory="game"`
  (`AndroidManifest.xml:97-98`) and `preferMinimalPostProcessing="true"`
  (`:122`). Turbo mode is already a setting (`SettingsSections.kt:779-786`)
  and is hidden only on Mali GPUs (`native_config.cpp:843-845`).
- **Input.** SDL reads the gamepads. `InputDeviceUtils.kt:24-51` only feeds
  the "controller connected" state in the UI. The app handles
  `KEYCODE_BACK` and `KEYCODE_BUTTON_MODE` itself (`Emulator.java:301-327`).
- **Launch.** `Emulator.java:171-185` reads the intent extra `title_id`.
  Activity: `org.vita3k.emulator.Emulator`.

## Decisions so far

- [01](issues/01-research-plus-and-local-branches.md): the local chain 1838cd88..feb21710 applies to `master`. 9 small Plus commits apply alone. The big Plus memory and kernel work needs 8be36fa1 and cannot be taken alone. Recommended for ticket 02: option 1, single commits.
- [02](issues/02-decide-code-base.md): keep `master` and cherry-pick (tickets 19 to 23). The ticket 11 present mode rule is used only on Android.

## Open questions

- The Ayaneo system button key codes. Ticket 17.
- Whether the device is CPU-bound or GPU-bound per title. Ticket 07.
- Whether ADPF is supported by the vendor on this device. Ticket 13.
- The core types and clocks of the 4 middle cores and 3 small cores.
  Ticket 06.
