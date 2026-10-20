# Jak and Uncharted visual comparison follow-up

The user reports better Jak and Uncharted visuals in Vita3K-Plus. Exact scenes,
build identities and actual drivers have been requested. No new device result
has been supplied during this follow-up. The public phase 1 release targets
d71bfc1f. It predates the selected Plus ports at c2f81a34. Those ports alone
also omit several relevant rendering paths.

Plus was fetched again without updating submodules. The all-enhancements tip
is still a86b5f93db43e4c4640ad1527e2a4695aad0f35b.

## Changes in this follow-up

- Port the raw half-float framebuffer input from Plus 24702197. Read unsigned
  components from input attachment 1, binding 2, when a fragment program reads
  an F16 colour surface. Keep the ordinary float branch. Match the descriptor
  layout, descriptor pool counts, image usage, subpass inputs and both image
  barriers. Use Plus's distinct in-memory hash and disk version suffix for
  this variant. Cache version 20 invalidates the older products. Keep our
  existing precision, sRGB and output register handling. Do not import the
  same commit's separate Turnip stencil/LRZ workaround.
- Adapt the partial write-back work from e530c4e8 and smaller-rebind bounds
  from 7c52fc78. Record the scene rectangle before the tile-rounded union.
  Eligible small linear surfaces copy only their drawn rectangle instead of
  discarding the update. Keep the local render-target clamp. Keep tiled,
  packed and CPU-swizzled formats out of this new partial path. Smaller
  rebinds skip those conversion paths because their CPU loops still process
  the whole cached image. Direct linear copies intersect the bound extent.
- Give Double Buffer the exact rectangle actually copied by the GPU. Calculate
  its byte span as (rows - 1) * stride + row_bytes. Do not add the whole old
  surface size to an offset rectangle address. Validate row layouts and use
  64-bit bounds arithmetic before copying the mapped data into guest memory.
  Page Table and other direct mappings retain their GPU copy path.
- Port the CPU-written presentation subset of 766e0d2b. Track transfer
  destinations. When the cached render is at least two frames old and the CPU
  content is newer, use guest-memory presentation. Preserve Plus's two-frame
  policy; it is not a newly measured timeout. Reset the marker on cache reuse.
  Guard timestamp subtraction and use 64-bit address containment.

The framebuffer correction takes priority over proposals to remove input
attachments for performance. Removing its required raw attachment would
reintroduce the missing input. GMEM residency and cost must be measured on the
device after correctness. No driver-specific workaround is enabled here.

## Automated checks

The shader tests translate the project's own color_f.gxp with framebuffer-read
metadata. Inspect actual SPIR-V instructions and decorations. Require a uint
image read at the raw attachment binding and the float fallback; require only
the float read for the ordinary variant. This does not execute a GPU shader.

Surface tests check that partial row copies preserve surrounding CPU bytes,
reject invalid row layouts without writes, exclude trailing padding, avoid
32-bit span overflow, intersect smaller bounds and retain the presentation
policy across CPU writes and a subsequent GPU render.

Build and test results are recorded below after verification. No physical GPU,
gameplay, visual parity or performance result can be inferred from them.

## Remaining visual work

This is a selective port. GPU photo read-back from 93639d0b remains pending,
including synchronous Double Buffer ownership. The newer vertex stream
alignment fix requires its matching repacking/binding path. Jak's remaining
GXM wait, JPEG writer coordination and memory placement changes need a
separate contract and lifetime pass. Existing NGS guest-address lifetime fixes
must survive that work. Do not describe this APK as containing every Plus fix.

Use testing.md for a matched build, driver, settings and scene comparison.

## Verification completed before the Android CI build

The Linux x86-64 RelWithDebInfo application build passed. All 110 C++ tests
across nine CTest entries passed in 5.63 seconds. This includes the two new
SPIR-V translation checks and five surface synchronization checks. All 27
Python performance-tool tests passed.

The packaging helper was exercised against the existing c2f81a34 verification
APK. Signature, 16 KiB ZIP alignment, all 57 assets compared byte for byte,
expected native build label and matching unstripped symbols passed. This
checks the helper; it does not build or validate the new Android application.
The fresh GitHub Actions build supplies the new APK and its own manifest.
