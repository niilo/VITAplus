Download **@APK@**. It installs as **@LABEL@**, package `@PACKAGE@`.
This prerelease includes the earlier selected Plus ports and additional Vulkan
visual fixes absent from the phase 1 APK:

- Raw half-float framebuffer input during programmable blending, with matching
  input attachments, descriptors and barriers. Shader cache version 20 gives
  these shader variants separate entries and rebuilds older products.
- Partial surface write-back for eligible linear surfaces. Preserve CPU bytes
  outside the copied rows and bound copies when a cached surface is rebound
  smaller. Keep the existing packed-format and tiled-surface restrictions.
- Plus's CPU-written display-buffer fallback for idle cached surfaces, relevant
  to Jak menus and animation.
- Previous ports include ARM variadic doubles, packed IMAD16, JPEG pitch/crop,
  YVU plane order, file-read preservation and Vulkan cache validation.

Import your intended Vulkan driver into this app. Its settings and driver
storage are separate. Match the Plus app's driver and settings, compare the
same Jak and Uncharted scenes at 1x, then 3x, and repeat after a restart.
See TESTING.md for the comparison procedure and remaining gaps.

Source: `@SOURCE@`. Application code reference: `@APPLICATION_SOURCE@`.
Native build ID: `@NATIVE_ID@`.
APK SHA-256: `@APK_SHA256@`.

The Linux build and automated regressions are checked separately from device
rendering. The APK build verifies signing, 16 KiB ZIP alignment, packaged assets
and matching profiling symbols. Physical Pocket S and Steam Deck rendering,
performance, power and thermals remain unmeasured. The GPU photo read-back,
vertex alignment and remaining Jak synchronization changes are still pending.
This test build does not establish complete visual parity or 60 FPS at 3x.
