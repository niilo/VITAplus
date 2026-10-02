# 05: Measure the render configuration matrix

Status: open
Type: experiment
Label: ready-for-human
Blocked by: 04

## Goal

Six settings changed their defaults when the base moved to Plus, and none has
been measured on this device. Each one changes how much work the GPU or the
CPU does. Find the combination that is fastest on this hardware.

This ticket changes no code. It runs the protocol from `../spec.md` over
settings that already exist.

## The settings

| Setting | Default here | What it changes |
|---|---|---|
| `high-accuracy` | `true` | Off: texture viewport on (`renderer.cpp:1088`), subpass-input framebuffer fetch. On: shader interlock, which restarts the render pass per draw (`scene.cpp:463`), and texture viewport off. |
| `memory-mapping` | `page-table` | Turnip only. On the stock driver this is forced to `double-buffer` (`renderer.cpp:1112`). `double-buffer` copies guest memory into the GPU-visible buffer per vertex, index and uniform range (`renderer.cpp:2396`, `:2415`, `:2474`). |
| `disable-surface-sync` | `false` | Off means `perform_surface_sync()` runs at the end of every scene (`vulkan/context.cpp:610`), which ends the scene with a `vkCmdCopyImageToBuffer` (`surface_cache.cpp:2608`). A game that reads back its own color surface breaks with this on. |
| `surface-sync-clamp-rt` | `true` | Clamps the write-back region to what was drawn. Directly reduces the bytes that step 3 copies. |
| `disable-programmable-blending` | `false` | Turns off the subpass-input framebuffer fetch path. On a tile renderer a framebuffer fetch also disables LRZ. |
| `guest-cores` | `3` | How many guest threads run at once. Lower can raise FPS and hurt latency. It has no field in `EmulatorConfig.kt`, so `device.sh config-set` is the only route. |
| `accurate-thread-scheduling` | `true` | Software scheduling in `kernel/src/thread.cpp`. |
| `preempt-on-wake` | `false` | Preempts a guest thread on wake, with `preempt-on-wake-us` at 1000. |
| `async-pipeline-compilation` | `false` | Whether pipeline compiles move off the render thread. |
| `log-level` | `2` | Plus logs a line before every pipeline compile on the stock Adreno driver (`pipeline_cache.cpp:1210`). Ticket 17 times that line separately. |
| `turbo-mode` | `false` | Calls `adrenotools_set_turbo` (`renderer.cpp:2388`). It only exists for the stock Qualcomm driver. The call is not present on the other driver. |
| `disable-raster-order` | `false` | Turns off `VK_EXT/ARM_rasterization_order_attachment_access`, which **is present here** under Turnip. It is the framebuffer-fetch path this device uses, so this setting is live and worth an A/B. |

## What is already measured, on this device

Three settings have been tested with the gameplay harness, on Uncharted at
resolution 2, camera moving, read with `--warmup 120`:

| Setting | Driver | FPS | p99 | Result |
| --- | --- | --- | --- | --- |
| `high-accuracy: false` | stock | 5.76 | 187.91 ms | baseline for that driver |
| `high-accuracy: true` | stock | 6.39 | 169.84 ms | **no effect**, `direct_fragcolor` stays true |
| `high-accuracy: false` | Turnip | 29.96 | 43.43 ms | baseline for that driver |
| `disable-programmable-blending: true` | Turnip | 29.86 | 47.65 ms | **no gain, and it breaks the picture** |

The last one is the important negative result. Turning programmable blending
emulation off does not make the framebuffer-fetch path cheaper on Turnip,
because Turnip was never using the expensive path: it has
`rasterization_order_attachment_access`, which needs no per-draw barrier.

And it destroys the picture. The screenshot at
`tmp/gameplay/turnip-nopb/step-4-level.png` shows large black polygons where
the waterfall and the rock face are, and the file is 825 KB against 6.4 MB for
a correct frame. The feature log says why:

```
Programmable blending emulation disabled by config (framebuffer fetch will read nothing)
FeatureState: direct_fragcolor=false programmable_blending=false
```

The setting is **`rejected`** for this device. It buys nothing and it costs the
picture. NetherSX2-Turnip documented the same lever fixing a PS2 game, which
does not carry over here.

So the settings that are left worth measuring in this ticket are
`disable-surface-sync`, `surface-sync-clamp-rt`, `guest-cores`,
`accurate-thread-scheduling`, `async-pipeline-compilation`, `log-level` and
`turbo-mode`. The framebuffer-fetch rows are answered by ticket 00 and ticket 06.

## Steps

1. Ticket 01 found `support_rasterized_order_access: true` under Turnip, so
   `disable-raster-order` takes effect. On the stock driver the extension is
   not documented; read ticket 00 for the driver before running.
2. Run a full A/B set per setting, one setting at a time, from the baseline
   values. Then run the best three together against the baseline.
3. For `memory-mapping`, run the full matrix on Turnip. On the stock driver only
   `double-buffer` and `external-host` are reachable, and ticket 01 records
   whether `external-host` is available at all (it needs
   `VK_EXT_external_memory_host`, which neither driver has).
4. Record for every run: the `perf_report.py` table from ticket 04, plus
   `scenes.csv` draw counts. A setting that raises FPS but also raises the
   frame interval 99th percentile is a latency regression, and criterion 2
   covers that.
5. For `high-accuracy`, check the picture too. Turning it off enables texture
   viewport, which the code comment calls "faster but not entirely accurate".
   Take one screenshot at each of three fixed save-slot moments in A and again
   in B, with `tools/android/device.sh screenshot`, and compare them under
   `tmp/pocket-s-android13/05/`. Write down the first visible difference, or
   `no difference seen`.
6. For `disable-surface-sync` and `surface-sync-clamp-rt`, check the picture
   the same way. Both change what is written back to guest memory, and a game
   that reads back its own color surface shows stale or missing content rather
   than a crash.
7. **State whether each title is CPU-bound or GPU-bound.** For each title, use
   `gpu_busy_percentage` mean and `task-clock` per frame from ticket 02, plus
   the renderer thread's share of `cpu-cycles` from a simpleperf report from
   ticket 03. Call a title CPU-bound when the renderer thread's share of
   `cpu-cycles` is above 60% and `gpu_busy_percentage` is below 90%. Put the
   four verdicts in `../spec.md`. Ticket 25 is gated on them, and every later
   ticket reads its verdict from there.

## Acceptance

- Every setting has a number, a verdict and a recommended value.
- The recommended values are written into `../spec.md` as a table, so ticket 22
  can turn them into a preset without re-reading this ticket.
- The four CPU-or-GPU-boundness verdicts are in `../spec.md`.
- `disable-surface-sync: true` and `high-accuracy: false` are the two most
  likely to matter. If either is untested, say so.

## Answer

## Comments
