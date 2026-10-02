# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Writing standard

Every piece of English written for this project follows these rules. This
includes this file, `README.md`, commit messages, code comments, SQLite
entries, reports, UI strings, and replies to the user.

- Use literal, plain, direct language.
- Do not use metaphors, similes, analogies, or idioms. Examples of banned
  wording: "journey", "tapestry", "navigating", "beacon", "dive in",
  "landscape", "footgun", "bite", "chase", "going in circles", "under the
  hood".
- Do not use AI buzzwords, hype words, or decorative adjectives. State facts
  and concepts exactly as they are.
- Keep sentences short. Put one idea in each sentence. Order sentences
  logically.
- Prefer clarity and precision over style.
- When you edit existing text that breaks these rules, rewrite it so that it
  complies.

## What this repository is

This checkout is `niilo/Vita3K` (`origin`). Since 2026-09-29 the code base
is Vita3K-Plus (nckstwrt/Vita3K-Plus, read-only remote `plus`, branch
`plus/all-enhancements`), with our own work on top, on `master`. The reason
and the plan are in `.scratch/plus-base/`. The old code base (upstream
Vita3K with picked Plus commits) is the branch `pre-plus-master`.
`upstream` is `Vita3K/Vita3K`.

Plus does not commit `vita3k/util/include/util/fork_build.h`. The build
generates a default one (`vita3k/util/CMakeLists.txt`).

The Android app ID is `org.vita3k.emulator`, as in upstream. Plus uses
`org.vita3kplus.emulator`, so both apps can be installed side by side.

Unmerged Adreno work exists on `feat/ayaneo-pocket-s-performance`,
`feat/vulkan13-adreno` and `feat/vulkan-device-profiles`. It was merged into
the old `master`; for this base see `.scratch/plus-base/issues/06`.

The Vulkan validation layer is off by default in Plus (`config.h:144`). The
APK still carries it (`android/prebuilt/`, packaged through `jniLibs`).
Check for "Enabling vulkan validation layers" in `vita3k.log`. Measure
speed with release APKs.

`AGENTS.md` points to this file and holds no rules of its own.
`docs/agents/` configures the engineering skills: issues are Markdown files
under `.scratch/<feature-slug>/`, and domain docs are `CONTEXT.md` and
`docs/adr/` when they exist.

## Containers

Two drivers for the same Containerfiles. Use `container/vita3k.sh` with Apple's
`container` CLI on macOS, and `container/vita3k-docker.sh` with Docker or Podman
on Linux x86_64. Both take the same commands and mount the repo at `/src`, so
output lands in the normal `build/` folder on the host. `vita3k-docker.sh` needs
no device and no person, so an agent can build, test and format with it.
`docs/agent-loop.md` has the build, test and review loop for the tickets under
`.scratch/pocket-s-android13/`.

```sh
container/vita3k.sh build            # Linux build, preset container-linux
container/vita3k.sh test             # ctest; extra args go to ctest, e.g. -R mem
container/vita3k.sh format-check     # the same clang-format 22 check as CI
container/vita3k.sh format
container/vita3k.sh android          # debug APK, both ABIs: build/android-apk/app-reldebug.apk
container/vita3k.sh android release  # small APK, arm64 only, R8 on: build/android-apk/app-release.apk
container/vita3k.sh shell [android]  # interactive shell
container/vita3k.sh run <cmd...>     # any command in the Linux container
```

- `container/linux.Containerfile`: Fedora 44. Fedora is used because it ships
  Qt 6.11. Ubuntu 26.04 has only 6.10. `vita3k.sh` builds it arm64 for Apple
  silicon; `vita3k-docker.sh` builds it amd64.
- `container/android.Containerfile`: Ubuntu 24.04 amd64, because the NDK has
  x86_64 Linux host tools only. `vita3k.sh` runs it through Rosetta on Apple
  silicon. Keep its `ANDROID_NDK_VERSION` equal to `ndkVersion` in
  `android/app/build.gradle`.
- The reldebug APK is about 100 MB: R8 is off, so the Java code is about
  64 MB, and it has a second `libVita3K.so` for x86_64. The release APK has
  neither. `container/build-android.sh` holds both build modes.
- The image tag is a hash of the Containerfile. An edit to the file builds a
  new image on the next command. `vita3k-docker.sh` puts `docker` in the tag, so
  the two drivers never reuse each other's image.
- ccache, vcpkg binaries and the Gradle cache are kept in the volumes
  `vita3k-linux-ccache` and `vita3k-android-cache`
  (`clean-cache` deletes them).
- `VITA3K_CONTAINER_CPUS` and `VITA3K_CONTAINER_MEMORY` (default 16G) set the
  container size. The container default of 1 GiB is too small to link.
- `tools/android/device.sh` runs the test loop on an Android device through
  adb: install, launch, config changes, logs, thermal data. Run it with
  `help` for the commands.
  `device.sh hold <x> <y>` holds a touch for 300 ms. The games take that, but
  not a plain tap or an adb key event.
- `tools/android/uncharted_scene.sh <package> <label>` starts Uncharted:
  Golden Abyss, loads the saved chapter and samples the FPS counter into one
  picture (`tools/android/fps_sample.py`). Use it to compare a build, a Vulkan
  driver or a setting in the same scene. Needs an unlocked device. Results go
  to `tmp/uncharted-scene/<label>/`. Check `custom-driver-name` first: with the
  stock Qualcomm driver the scene runs at 7 FPS (2x) and with the Turnip
  driver at 30 FPS.
- `.signing/` holds the local signing keys (a dev key and the release key). Git
  ignores it and the repository is public. Never print, commit, copy, upload or
  send a file from it, and never put a password from it in a command line.
  `tools/release/check-no-signing-keys.sh` checks that no key is tracked (CI runs
  it on every push). `container/vita3k.sh android release` uses the dev key from
  `.signing/dev/` by itself, so an installed APK is always updatable without
  input. `VITA_SIGN_WITH_RELEASE_KEY=1` uses the release key. See
  `docs/release.md`.
- Test the Python tools with `python3 tools/android/test_fps_sample.py` and
  `python3 tools/android/test_perf_summary.py`.
- A container build cannot run the emulator with a GPU. Use a native macOS
  build (below) to run games.

## Build

After a clone or a branch change, update the submodules. Most dependencies
are submodules under `external/` (dynarmic, SDL, glslang, SPIRV-Cross, boost,
ffmpeg, googletest, and others).

```sh
git submodule update --init --recursive
```

CMake presets are named `<os>-<generator>-<compiler>`. Each one builds into
`build/<preset>`. Run `cmake --list-presets` to see the ones for this host.
The Qt frontend needs Qt 6.11 or newer. Set `Qt6_ROOT` if the system Qt is
older.

```sh
# macOS (this machine): brew install git cmake molten-vk openssl qt
cmake --preset macos-ninja
cmake --build build/macos-ninja --config RelWithDebInfo

# Linux
cmake --preset linux-ninja-clang
cmake --build build/linux-ninja-clang --config RelWithDebInfo

# Windows
set Qt6_ROOT=C:\Qt\6.11.0\msvc2022_64
cmake --preset windows-vs2022
cmake --build build/windows-vs2022 --config RelWithDebInfo
```

Android uses Gradle, with the native code built through CMake. It needs
`ANDROID_NDK_HOME`, `VCPKG_ROOT`, and the vcpkg manifest dependencies
(`vcpkg install --triplet arm64-android` from the repo root).

```sh
cd android && ./gradlew --stacktrace assembleReldebug
```

Target devices: Android on the Ayaneo Pocket S (arm64) and Linux on the Steam
Deck (x86_64). The code for Windows and macOS stays in the repository, but CI
does not build it and nobody tests it.

CI runs only when a release tag `v*` is pushed (`docs/release.md`). The workflow
`.github/workflows/c-cpp.yml` builds the Steam Deck AppImage through
`.ci/build-desktop.sh --preset ci-linux-clang-appimage --config Release` and the
signed Android APK through `.ci/build-android.sh`, then creates a GitHub release
with `.ci/release-collect.sh`. CodeQL runs on the same tags. The format check
(`.github/workflows/format.yml`) still runs on every push and pull request,
because it is not a build.

## Tests

Tests use googletest and CTest. Two suites exist: `mem-tests`
(`vita3k/mem/tests/`) and `module-tests` (`vita3k/module/tests/`).

```sh
ctest --test-dir build/<preset> --build-config RelWithDebInfo --output-on-failure
ctest --test-dir build/<preset> --build-config RelWithDebInfo -R mem   # one suite
build/<preset>/bin/<config>/mem-tests --gtest_filter='Suite.Name'      # one test; binary path depends on generator
```

## Format

CI checks formatting with clang-format on `vita3k/` and `tools/`
(`.clang-format` at the root). Format before a commit:

```sh
./format.sh            # CLANG_FORMAT_BIN=<path> to pick a binary
```

## Architecture

The emulator is under `vita3k/`. Each subdirectory is a CMake static library
with `include/<name>/` and `src/`. The main pieces and how they connect:

- **`emuenv/`**: `EmuEnvState` holds the state of the whole emulator (memory,
  kernel, IO, renderer, display, audio, config). Most code takes it as the
  first argument.
- **`cpu/`**: guest ARM CPU through dynarmic (`dynarmic_cpu.cpp`).
- **`mem/`**: the guest address space, a large host reservation with a page
  allocator. Memory mapping modes (External Host, Page Table, Native Buffer,
  Double Buffer) decide how the GPU sees guest memory.
- **`kernel/`**: guest threads, sync objects, waits, callbacks, and the module
  loader.
- **`module/`** and **`modules/`**: high-level emulation (HLE) of Vita system
  libraries. `modules/` has one folder per Vita library (`SceGxm`,
  `SceAudio`, `SceIofilemgr`, and about 150 more). A function is written as
  `EXPORT(ret, name, args...)` (`module/include/module/module.h`), and its
  NID is registered in `nids/include/nids/nids.inc` as
  `NID(name, 0x...)`. A new export needs both. Some libraries are loaded as
  the game's or firmware's own LLE (real code) modules instead;
  `modules/module_parent.cpp` handles loading and import binding.
- **`gxm/`** and **`renderer/`**: `SceGxm` calls build renderer commands
  (`renderer/include/renderer/commands.h`, `CommandOpcode`). A separate
  renderer thread runs them. Backends are `renderer/src/vulkan/` (main;
  `surface_cache.cpp` maps guest render targets and textures to host images)
  and `renderer/src/gl/`. `renderer/src/texture/` decodes Vita texture
  formats.
- **`shader/`**: translates GXP shader programs (USSE instructions) to SPIR-V
  (`spirv_recompiler.cpp`, `translator/`), and then to GLSL for OpenGL
  through SPIRV-Cross.
- **`io/`**: the virtual filesystem (`ux0:`, `app0:`, `vs0:`, and so on)
  mapped to host folders.
- **`display/`**: vblank timing and frame presentation.
- **`gui-qt/`**: the Qt desktop frontend. **`overlay/`**: the in-game
  overlay. **`android/jni/`** plus `android/app/` (Kotlin): the Android
  frontend.
- **`interface.cpp`** and **`main.cpp`**: boot flow (install or load an app,
  load modules, start the main thread).

`tools/gen-modules` generates module stubs. `tools/native-tool` and
`tools/usse-decoder-gen` are developer tools. `vita3k/shaders-builtin/` holds
the host shaders that the renderer needs at run time.
